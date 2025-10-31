#include <iostream>
#include <cstring>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    for (;;) {

        // need date/time, username, and absolute path to current dir

        size_t const SIZE = 256; // set buffer size
        char buf[SIZE]; // create buffer
        time_t timer = time(NULL); // set timer
        char* time_show = ctime(&timer); // get timer value
        time_show[strcspn(time_show, "\n")] = '\0'; // replace endline with null
        const char* user = getenv("USER"); // get the user enviornment
        if (!user) { // if there is not a user
            user = "root";  // default to root
        }
        char* pwd = getcwd(buf, sizeof(buf)); // now get the current working directory
        if (!pwd) { // if no current working directory
            perror("error getting current directory"); // issue
            pwd = const_cast<char*>("/"); // go to root instead
        }
        string finalTime(time_show);
        finalTime = finalTime.substr(4, 15); // get rid of the day of the week
        
        // cout << YELLOW << finalTime << " " << getenv("USER") << ":" << getcwd(buf, SIZE) << "$" << NC << " ";
        cout << user << " " << finalTime << " :" << pwd << "$"; // follow guidance from discord
        cout.flush();
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }


        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }

        static string dirStorage; // stores previous directory

        if(tknr.commands.at(0)->args.at(0) == "cd") { // if cd
            char currDir[SIZE]; // to store current directory
            getcwd(currDir, sizeof(currDir)); // gets the cwd current working directory
            if(tknr.commands.at(0)->args.size() == 1) {
                chdir(getenv("HOME"));
            } // if nothing dont try to access go home
            else if(tknr.commands.at(0)->args.at(1) == "-") { // go to previous directory
                if(!dirStorage.empty()) { // if previous directory
                    chdir(dirStorage.c_str()); // set the directory to the previous directory
                }
            }
            else {
                if(chdir(tknr.commands.at(0)->args.at(1).c_str()) != 0) // current directory the argument
                    perror("did not change directory correctly");
            }

            dirStorage = currDir; // store the current directory as the prev directory

            continue;
        }

        signal(SIGCHLD, SIG_IGN); // if child signal ignore it (reaps zombies)

        size_t cmdCount = tknr.commands.size();

        int* fds = new int[((cmdCount - 1) * 2)]; // creates place for pipe ***NEEED DYNAMIC ALLOCATION***
        for(size_t i = 0; i < cmdCount - 1; ++i) {
            if (pipe(fds + i * 2) == -1) { // pipes and checks if error while pipping
                cout << "Error Pipping \n";
                return 1; // error while pipping so exit entire program
            }
        }

        vector<pid_t> pids; // make a vector to store PIDs
        for(size_t i = 0; i < cmdCount; ++i) { // for each command
                // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if(!tknr.commands.at(i)->isBackground()) // if not background process
                pids.push_back(pid); // add pids to list
            
            if (pid == 0) {  // if child, exec to run command
                if(i > 0) dup2(fds[(i - 1) * 2], STDIN_FILENO); // set the fd's correctly from the input
                if(i < cmdCount - 1) dup2(fds[(i * 2) + 1], STDOUT_FILENO); // set the other fd for the output

                for(size_t j = 0; j < (cmdCount - 1) * 2; ++j) { // close everything
                    close(fds[j]);
                }

                if(tknr.commands.at(i)->hasInput()) { // if <
                    int inFile = open(tknr.commands.at(i)->in_file.c_str(), O_RDONLY); // open to read
                    if(inFile < 0) { // if error reading
                        perror("Unable to open file to read");
                        exit(2);
                    }
                    dup2(inFile, STDIN_FILENO); // set the input to the file
                    close(inFile); // close it
                }
                if(tknr.commands.at(i)->hasOutput()) { // if >
                    int outFile = open(tknr.commands.at(i)->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); // create file if one alr exists truncate also set permission bits
                    if(outFile < 0) { // error opening to write
                        perror("Unable to open file to write");
                        exit(2);
                    }
                    dup2(outFile, STDOUT_FILENO); // set output to file
                    close(outFile); //close
                }

                size_t size = tknr.commands.at(i)->args.size(); // get the size
                char** args = new char*[size+1]; // create a char* of the size + 1
                for(size_t j = 0; j < size; ++j) { // for each
                    args[j] = (char*) tknr.commands.at(i)->args.at(j).c_str(); // add the arguments to one args
                }
                args[size] = nullptr; // add null at end

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            }
        }
        
        for(size_t j = 0; j < (cmdCount - 1) * 2; ++j) { // close everything for the parent
            close(fds[j]);
        }

        int status = 0;
        if(tknr.commands.back()->isBackground()) { // if background don't care about checking
            printf("we in the background baby");
        }
        else { // wait
            while(!pids.empty()) { // use pid vector list to check
                waitpid(pids.back(), &status, 0);
                pids.pop_back(); // pop when reaping each process until all reaped
                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            }
        }

        delete[] fds; // dont forget cleanup
    }
}

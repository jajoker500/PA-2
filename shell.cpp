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

        size_t const SIZE = 256;
        char buf[SIZE];
        time_t timer = time(NULL);
        char* time_show = ctime(&timer);
        time_show[strcspn(time_show, "\n")] = '\0';
        const char* user = getenv("USER");
        if (!user) {
            user = "root";  // default if USER isn't set
        }
        char* pwd = getcwd(buf, sizeof(buf));
        if (!pwd) {
            perror("getcwd failed");
            pwd = const_cast<char*>("/"); // fallback to root
        }
        string finalTime(time_show);
        finalTime = finalTime.substr(4, 15);
        
        // cout << YELLOW << finalTime << " " << getenv("USER") << ":" << getcwd(buf, SIZE) << "$" << NC << " ";
        cout << user << " " << finalTime << " :" << pwd << "$"; 
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
            } // *** finish commenting after submission ***
        }

        vector<pid_t> pids;
        for(size_t i = 0; i < cmdCount; ++i) {
                // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if(!tknr.commands.at(i)->isBackground()) 
                pids.push_back(pid);
            
            if (pid == 0) {  // if child, exec to run command
                if(i > 0) dup2(fds[(i - 1) * 2], STDIN_FILENO);
                if(i < cmdCount - 1) dup2(fds[(i * 2) + 1], STDOUT_FILENO);

                for(size_t j = 0; j < (cmdCount - 1) * 2; ++j) {
                    close(fds[j]);
                }

                if(tknr.commands.at(i)->hasInput()) {
                    int inFile = open(tknr.commands.at(i)->in_file.c_str(), O_RDONLY);
                    if(inFile < 0) {
                        perror("Unable to open file to read");
                        exit(2);
                    }
                    dup2(inFile, STDIN_FILENO);
                    close(inFile);
                }
                if(tknr.commands.at(i)->hasOutput()) {
                    int outFile = open(tknr.commands.at(i)->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if(outFile < 0) {
                        perror("Unable to open file to write");
                        exit(2);
                    }
                    dup2(outFile, STDOUT_FILENO);
                    close(outFile);
                }

                size_t size = tknr.commands.at(i)->args.size();
                char** args = new char*[size+1];
                for(size_t j = 0; j < size; ++j) {
                    args[j] = (char*) tknr.commands.at(i)->args.at(j).c_str();
                }
                args[size] = nullptr;

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            }
        }
        
        for(size_t j = 0; j < (cmdCount - 1) * 2; ++j) {
            close(fds[j]);
        }

        int status = 0;
        if(tknr.commands.back()->isBackground()) {
            printf("we in the background baby");
        }
        else {
            while(!pids.empty()) {
                waitpid(pids.back(), &status, 0);
                pids.pop_back();
                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            }
        }

        delete[] fds;
    }
}

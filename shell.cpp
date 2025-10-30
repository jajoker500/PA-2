#include <iostream>
#include <cstring>

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
        time_show[strcspn(time_show, "\n")] = ' ';
        
        cout << YELLOW << time_show << getenv("USER") << ":" << getcwd(buf, SIZE) << "$" << NC << " ";
        
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

        size_t cmdCount = tknr.commands.size();

        int fds[((cmdCount - 1) * 2)]; // creates place for pipe ***NEEED DYNAMIC ALLOCATION***
        for(int i = 0; i < cmdCount - 1; ++i) {
            if (pipe(fds + i * 2) == -1) { // pipes and checks if error while pipping
                cout << "Error Pipping \n";
                return 1; // error while pipping so exit entire program
            }
        }
        
        for(size_t i = 0; i < cmdCount; ++i) {
                // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                if(i > 0) dup2(fds[(i - 1) * 2], STDIN_FILENO);
                if(i < cmdCount - 1) dup2(fds[(i * 2) + 1], STDOUT_FILENO);

                for(int j = 0; j < (cmdCount - 1) * 2; ++j) {
                    close(fds[j]);
                }

                if(tknr.commands.at(i)->hasInput()) {
                    dup2(fds[i], STDIN_FILENO); // replace standard input with read end of pipe
                    close(fds[i + 1]); // close write end of pipe
                }
                else if(tknr.commands.at(i)->hasOutput()) {
                    dup2(fds[i + 1], STDOUT_FILENO); // redirect the standard output to the write of the pipe
                    close(fds[i]); // close read of pipe
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

            else {  // if parent, wait for child to finish
                close(fds[i]); // close read
                close(fds[i + 1]); // close write of pipe
                int status = 0;
                waitpid(pid, &status, 0);
                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            }
        }
    }
}

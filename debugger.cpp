#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <vector>
#include <string.h>
// #include "linenoise/linenoise.h"

using std::cout; using std::string; using std::vector;

enum {
    DEBUGEE = 0,  // pid for child process
    DEBUGGER = 1  // pid for parent process
};

class Debugger {
    string program_name;
    pid_t pid;
    vector<string> command_list;

    public:
        Debugger(char *prog_name, pid_t id) {
            program_name = prog_name;
            pid = id;

            int num_commands = 2;
            string commands[] = {"quit", "continue"};
            for (int i = 0; i < num_commands; i++) {
                command_list.push_back(commands[i]);
            }
        }

        void run() {
            cout << "Running program\n";

            int wait_status;
            int options = 0;
            waitpid(pid, &wait_status, options);

            // Now that we've stopped, we can allow the user to enter as many commands as they want
            while (true) {
                cout << "(debugger) ";
                string line;
                getline(std::cin, line, '\n');
                handle_command(line);
            }
        }

        void handle_command(string line) {
            vector<string> tokens = split(line, " ");
            string command = is_command(tokens[0]);

            // Perform an action depending on the type of command entered by the user
            if (command == "quit") {
                exit(EXIT_SUCCESS);
            } else if (command == "continue") {
                cout << "Continuing execution now\n";
                continue_execution();
            }
        }

        string is_command(string token) {
            // Checks if a token is a valid command, and if it is, return that command

            // Loop through the list of valid commands
            for (int i = 0; i < command_list.size(); i++) {
                string command = command_list[i];
                if (token == command || (token.size() == 1 && token[0] == command[0])) {
                    // If the token matches one of the commands or it is a single character
                    // which matches the first character of one of the commands, return true
                    return command;
                }
            }

            // If we made it to this point, the token doesn't match any of the commands, so it must not be a command
            return "not a command";
        }

        // https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c#comment44856986_14266139
        vector<string> split(string line, string delimiter) {
            vector<string> out;

            size_t last = 0; size_t next = 0;
            while ((next = line.find(delimiter, last)) != string::npos) {
               out.push_back(line.substr(last, next-last));
               last = next + 1;
            }
            out.push_back(line.substr(last));

            return out;
        }

        void continue_execution() {
            ptrace(PT_CONTINUE, pid, nullptr, 0);

            int wait_status;
            int options = 0;
            waitpid(pid, &wait_status, options);
        }
};

int main(int argc, char *argv[]) {

    // If the user hasn't given a program name to debug, we need to exit with an error
    if (argc < 2) {
        std::cerr << "No program given to debug\n";
        return EXIT_FAILURE;
    }

    // Otherwise, the second command-line argument is the filename of the program to debug
    char *program_name = argv[1];
    // cout << "Program name: " << program_name << '\n';

    // Create two processes - one for the program being debugged, and one for the debugger
    pid_t pid = fork();

    if (pid == DEBUGEE) {
        // execute program being debugged
        // cout << "Child process " << pid << ", execute debugee\n";

        // Allow the parent process to control this process
        long code = ptrace(PT_TRACE_ME, DEBUGEE, nullptr, 0);
        /*
        if (code == ESRCH || code == EINVAL || code == EBUSY || code == EPERM || code = -1) {
            std::cerr << "Unable to take control of debugee\n";
        }
        */
        execl(program_name, program_name);

    }

    if (pid >= DEBUGGER) {
        // execute debugger
        // cout << "Parent process " << pid << ", execute debugger\n";

        Debugger debug(program_name, pid);
        debug.run();
    }

    return 0;
}

void handle_command(char *line) {
    cout << line << '\n';
}
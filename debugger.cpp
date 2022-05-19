#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <vector>
#include <unordered_map>
#include <string.h>

using std::cout; using std::cin; using std::cerr;
using std::string;
using std::vector; using std::unordered_map;

enum {
    DEBUGEE = 0,  // pid for child process
    DEBUGGER = 1  // pid for parent process
};

class Breakpoint {
        pid_t pid;
        caddr_t address;
        bool enabled;
        uint8_t saved_data;

    public:
        Breakpoint(pid_t p, caddr_t a) {
            pid = p;
            address = a;
            enabled = false;
            saved_data = 0;
        }

        Breakpoint() {
            enabled = false;
        }

        void enable() {
            // Read data from the breakpoint address
            int data = ptrace(PT_READ_D, pid, address, 0);

            // Save the least significant byte from that data
            saved_data = static_cast<uint8_t>(data & 0xff);

            // Set that same least significant byte from that data to 0xcc (int3)
            uint64_t int3 = 0xcc;
            uint64_t data_with_int3 = ((data & ~0xff) | int3);

            // Write this updated data back to the same address
            ptrace(PT_WRITE_D, pid, address, data_with_int3);

            enabled = true;
        }

        void disable() {
            // Read data from the breakpoint address
            int data = ptrace(PT_READ_D, pid, address, 0);

            // Remove the 0xcc from the least significant byte and replace it with the original data
            uint64_t restored_data = ((data & ~0xff) | saved_data);

            // Write the restored data back to the same address
            ptrace(PT_WRITE_D, pid, address, restored_data);

            enabled = false;
        }

        bool is_enabled() {
            return enabled;
        }

        caddr_t get_address() {
            return address;
        }
};

class Debugger {
    string program_name;
    pid_t pid;
    vector<string> command_list;
    unordered_map<string, Breakpoint> breakpoints;

    public:
        Debugger(char *prog_name, pid_t id) {
            /* Construct the Debugger object with the name of the program to be debugged
               and the process ID of the debugger */
            program_name = prog_name;
            pid = id;

            // Create a vector of valid commands to be checked against every time the user tries to enter a command
            int num_commands = 3;
            string commands[] = {"quit", "continue", "break"};
            for (int i = 0; i < num_commands; i++) {
                command_list.push_back(commands[i]);
            }
        }

        void run() {
            // Begin execution of the *debugger*, not the program to be debugged
            cout << "Running program\n";

            // Wait until control is passed back to the debugger process
            int wait_status;
            int options = 0;
            waitpid(pid, &wait_status, options);

            // Now that we've stopped, we can allow the user to enter as many commands as they want
            while (true) {
                cout << "(debugger) ";
                string line;
                getline(cin, line, '\n');
                handle_command(line);
            }
        }

        void handle_command(string line) {
            /* Given a line input from the user, check if they have entered a valid command,
               and if they have, perform it */

            // Split the input line into individual 'tokens'
            vector<string> tokens = split(line, " ");
            // Get the name of the command being used (the first token input by the user)
            string command = is_command(tokens[0]);

            /* Perform an action depending on the type of command entered by the user
               (if invalid, command = "not a command") */
            if (command == "quit") {
                exit(EXIT_SUCCESS);
            } else if (command == "continue") {
                cout << "Continuing execution now\n";
                continue_execution();
            } else if (command == "break") {
                const char *addr = tokens[1].c_str();
                set_breakpoint(addr);
            } else {
                cout << "Not a recognized command\n";
            }
        }

        string is_command(string token) {
            // Checks if a token is a valid command, and if it is, return that command

            bool match = true;

            // Loop through the list of valid commands
            for (int i = 0; i < command_list.size(); i++) {
                string command = command_list[i];
                match = true;

                // Loop through each character in the token and compare with the current command
                /* If each character in the token matches the corresponding character in the command,
                   we can consider them a match. Otherwise, move on to the next command */
                for (int c = 0; c < token.size(); c++) {
                    if (token[c] != command[c]) {
                        match = false;
                        break;
                    }
                }

                // This means the user doesn't have to write the entire command when they enter it
                /* For example, if the user wants to enter the "continue" command,
                   they could enter "continue", or "contin", or "cont", or "c", or anything in between */
                if (match) {
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
            /* Allow the debugee process to continue executing the program
               until control is passed back to the debugger process */
            // (caddr_t)1 tells ptrace to allow the debugee process to continue executing from where it left off
            ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);

            int wait_status;
            int options = 0;
            waitpid(pid, &wait_status, options);
        }

        void set_breakpoint(const char *b_addr) {
            cout << "Set breakpoint at address 0x" << std::hex << b_addr << '\n';

            // char *addr;
            // strcpy(addr, b_addr);
            // void *v_addr = addr;
            // caddr_t c_addr = (caddr_t)v_addr;

            // Create a new Breakpoint object at the given address, enable it, and store it in the map of breakpoints
            Breakpoint new_breakpoint(pid, (caddr_t)b_addr);
            new_breakpoint.enable();
            string str_addr(b_addr);
            breakpoints[str_addr] = new_breakpoint;
        }
};

int main(int argc, char *argv[]) {

    // If the user hasn't given a program name to debug, we need to exit with an error
    if (argc < 2) {
        cerr << "No program given to debug\n";
        return EXIT_FAILURE;
    }

    // Otherwise, the second command-line argument is the filename of the program to debug
    char *program_name = argv[1];

    // Create two processes - one for the program being debugged, and one for the debugger
    pid_t pid = fork();

    if (pid == DEBUGEE) {
        // Execute program being debugged

        // Allow the parent process to control this process
        long code = ptrace(PT_TRACE_ME, DEBUGEE, nullptr, 0);
        execl(program_name, program_name, (char *)0);
    }

    if (pid >= DEBUGGER) {
        // Execute debugger

        Debugger debug(program_name, pid);
        debug.run();
    }

    return 0;
}
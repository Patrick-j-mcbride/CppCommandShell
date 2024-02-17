#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <string>

using namespace std;

// Helper functions declarations
void execute_command(const vector<string> &args);

void execute_external(const vector<string> &args);

vector<string> parse_command(const string &input);

void handle_redirection(vector<string> &tokens);

vector<string>::iterator find_redirection_operator(vector<string> &tokens);

int is_env_assignment(const string &input);

void execute_cd(const vector<string> &args);

void execute_env_assignment(const vector<string> &args);

int main(int argc, char **argv) {
    // Check command-line arguments to decide the mode of operation
    if (argc == 1) {
        while (true) {
            string input; // Input line

            cout << "Mish:" << getcwd(nullptr, 0) << "$ " << flush; // Print prompt
            getline(cin, input); // Read input line

            if (!input.empty()) { // Check if the input line is not empty
                // Save the original stdout and stderr file descriptors
                int orig_stdout_fd = dup(STDOUT_FILENO);
                int orig_stderr_fd = dup(STDERR_FILENO);
                vector<string> args = parse_command(input); // Parse the input line
                execute_command(args); // Execute the command
                // Restore the original stdout and stderr file descriptors
                dup2(orig_stdout_fd, STDOUT_FILENO);
                dup2(orig_stderr_fd, STDERR_FILENO);
                // Close the duplicated file descriptors
                close(orig_stdout_fd);
                close(orig_stderr_fd);
            }
        }
    } else if (argc == 2) {
        // Filename provided, run commands from the file (non-interactive mode)
        ifstream file(argv[1]);
        if (file.is_open()) {
            string line; // Line read from the file
            while (getline(file, line)) { // Read line by line from the file until EOF
                if (!line.empty()) { // Check if the line is not empty
                    // Save the original stdout and stderr file descriptors
                    int orig_stdout_fd = dup(STDOUT_FILENO);
                    int orig_stderr_fd = dup(STDERR_FILENO);
                    vector<string> args = parse_command(line); // Parse the line
                    execute_command(args); // Execute the command
                    // Restore the original stdout and stderr file descriptors
                    dup2(orig_stdout_fd, STDOUT_FILENO);
                    dup2(orig_stderr_fd, STDERR_FILENO);
                    // Close the duplicated file descriptors
                    close(orig_stdout_fd);
                    close(orig_stderr_fd);
                }
            }
            file.close(); // Close the file
        } else { // Failed to open the file
            cerr << "Failed to open file: " << strerror(errno) << endl << flush; // Print error message
            exit(EXIT_FAILURE); // Exit with an error
        }
    } else {
        // More than one argument provided
        cerr << "Usage: " << argv[0] << " [script file]" << endl << flush; // Print usage
        exit(EXIT_FAILURE); // Exit with an error
    }
}

int is_env_assignment(const string &input) {
    //check if the command is an environment variable assignment
    regex pattern("^.+?=.+?$"); // Match any string followed by an equal sign and another string
    return regex_match(input, pattern);
}

// Split the input line into command and arguments
vector<string> parse_command(const string &input) {
    istringstream iss(input); // Create an input string stream
    vector<string> tokens; // Vector to store the tokens
    string token; // Temporary string to store each token

    while (iss >> token) { // Read each token separated by whitespace
        tokens.push_back(token); // Add the token to the vector
    }

    // Handle I/O redirection if present
    handle_redirection(tokens); // Handle I/O redirection
    return tokens;
}

// Execute a command with its arguments
void execute_command(const vector<string> &args) {
    if (args.empty()) return; // If no arguments provided, return

    // Check for built-in commands
    if (args[0] == "exit") {
        // Check if there are any arguments to the exit command, if so print an error message and ignore the command
        if (args.size() > 1) { // If there are more than one argument
            cerr << "exit: Too many arguments\n";
        } else {
            exit(0); // Exit the program
        }
    } else if (args[0] == "cd") { // Check if the command is "cd"
        execute_cd(args);
    } else if (args[0] == "export") { // Check if the command is "export"
        // Remove the "export" command and execute the rest as an environment variable assignment
        vector<string> env_args(args.begin() + 1, args.end());
        // Check if there are any arguments to the export command
        // If the first argument is an environment variable assignment, execute it
        if (!env_args.empty() && is_env_assignment(env_args[0])) {
            execute_env_assignment(env_args);
        } else {
            cerr << "export: Incorrect format. Use: export VAR=value\n";
        }
    } else if (is_env_assignment(args[0])) { // Check if the command is an environment variable assignment
        execute_env_assignment(args);
    } else if (args[0] == "echo") { // Check if the command is "echo"
        // Check if the second argument is an environment variable by checking if it starts with a dollar sign
        if (args.size() > 1 && args[1][0] == '$') {
            char *env_var = getenv(args[1].substr(1).c_str());
            if (env_var != nullptr) {
                cout << env_var << endl;
            }
        } else { // If the second argument is not an environment variable, call the external echo command
            execute_external(args);
        }
    } else {
        execute_external(args); // Execute an external command
    }
}

void execute_env_assignment(const vector<string> &args) {
    size_t eq_pos = args[0].find('='); // Find the position of the equal sign
    if (eq_pos != string::npos) { // If the equal sign is found
        string var = args[0].substr(0, eq_pos);
        string val = args[0].substr(eq_pos + 1);
        if (setenv(var.c_str(), val.c_str(), 1) != 0) {
            cerr << "export: " << strerror(errno) << endl;
        }
    } else {
        cerr << "Incorrect format. Use VAR=value\n";
    }
}

void execute_cd(const vector<string> &args) {
    if (args.size() > 2) { // If there are more than one argument
        cerr << "cd: Too many arguments\n";
    } else if (args.size() == 1) { // No arguments provided, print error message and ignore the command
        // No arguments provided, print error message and ignore the command
        cerr << "cd: Missing argument\n" << flush;
    } else { // Change the directory to the specified path
        const char *dir = (args.size() == 2) ? args[1].c_str() : getenv("HOME");
        if (chdir(dir) != 0) {
            cerr << "cd: " << strerror(errno) << endl;
        }
    }
}

// Handles I/O redirection in a given command
void handle_redirection(vector<string> &tokens) {
    auto it = find_redirection_operator(tokens); // Find the first redirection operator
    while (it != tokens.end()) {
        if (next(it) == tokens.end()) {
            cerr << "Redirection error: Missing filename\n";
            return;
        }

        string op = *it;
        string filename = *next(it);
        tokens.erase(it); // Remove redirection operator
        it = tokens.erase(it); // Remove filename and update iterator

        int flags = O_WRONLY | O_CREAT;
        if (op == ">") {
            flags |= O_TRUNC;
        } else if (op == ">>") {
            flags |= O_APPEND;
        } else if (op == "<") {
            flags = O_RDONLY;
        }

        int fd = open(filename.c_str(), flags, 0644);
        if (fd == -1) {
            cerr << "Redirection error: " << strerror(errno) << endl;
            return;
        }

        if (op == "<") {
            dup2(fd, STDIN_FILENO);
        } else {
            dup2(fd, STDOUT_FILENO);
        }
        close(fd);

        it = find_redirection_operator(tokens); // Check if there are more redirections
    }
}

// Finds redirection operators (>, >>, <) position in the command tokens
vector<string>::iterator find_redirection_operator(vector<string> &tokens) {
    return find_if(tokens.begin(), tokens.end(),
                   [](const string &s) { return s == ">" || s == ">>" || s == "<"; });
}

// Execute an external command
void execute_external(const vector<string> &args) {
    pid_t pid = fork(); // Create a new process

    if (pid == 0) { // Child process
        char *argv[args.size() + 1];
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i] = const_cast<char *>(args[i].c_str());
        }
        argv[args.size()] = nullptr;

        execvp(argv[0], argv); // Execute the command
        cerr << "Command not found: " << string(argv[0]) << endl << flush;
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        wait(nullptr); // Wait for the child process to finish
    } else {
        cerr << "Failed to create a new process\n";
    }
}
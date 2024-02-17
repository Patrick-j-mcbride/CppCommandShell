#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <limits.h>

using namespace std;

// Helper functions declarations
void execute_command(const vector<string>& args);
void execute_external(const vector<string>& args);
vector<string> parse_command(const string& input);
void handle_redirection(vector<string>& tokens);
vector<string>::iterator find_redirection_operator(vector<string>& tokens);
int is_env_assignment(const string& input);
void execute_cd(const vector<string>& args);
void execute_env_assignment(const vector<string>& args);

int main(int argc, char **argv) {
    // Check command-line arguments to decide the mode of operation
    if (argc == 1) {
        while (true) {
            string input;

            cout << "Mish:" << getcwd(nullptr, 0) << "$ ";
            getline(cin, input);

            if (input == "exit") {
                exit(0);
            }

            if (!input.empty()) {
                vector<string> args = parse_command(input);
                execute_command(args);
            }
        }
    } else if (argc == 2) {
        // Filename provided, run commands from the file (non-interactive mode)
        ifstream file(argv[1]);
        if (file.is_open()) {
            string line;
            while (getline(file, line)) {
                if (!line.empty()) {
                    vector<string> args = parse_command(line);
                    execute_command(args);
                }
            }
            file.close();
        } else {
            cerr << "Failed to open file: " << strerror(errno) << endl;
        }
    } else {
        // More than one argument provided, print usage and exit with an error
        fprintf(stderr, "Usage: %s [script file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    return 0;
}

int is_env_assignment(const string& input) {
    //check if the command is an environment variable assignment
    return input.find('=') != string::npos;
}

// Split the input line into command and arguments
vector<string> parse_command(const string& input) {
    istringstream iss(input);
    vector<string> tokens;
    string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    // Handle I/O redirection if present
    handle_redirection(tokens);
    return tokens;
}

// Execute a command with its arguments
void execute_command(const vector<string>& args) {
    if (args.empty()) return;

    // Check for built-in commands
    if (args[0] == "cd") {
        execute_cd(args);
    } else if (args[0] == "export") {
        // Remove the "export" command and execute the rest as an environment variable assignment
        vector<string> env_args(args.begin() + 1, args.end());
        execute_env_assignment(env_args);
    } else if (is_env_assignment(args[0])) {
        execute_env_assignment(args);
    } else if (args[0] == "echo") {
        // Check if the second argument is an environment variable by checking if it starts with a dollar sign
        if (args.size() > 1 && args[1][0] == '$') {
            char* env_var = getenv(args[1].substr(1).c_str());
            if (env_var != nullptr) {
                cout << env_var << endl;
            }
        } else {
            for (size_t i = 1; i < args.size(); ++i) {
                cout << args[i] << " ";
            }
            cout << endl;
        }
    } else {
        execute_external(args);
    }
}

void execute_env_assignment(const vector<string>& args) {
    size_t eq_pos = args[0].find('=');
    if (eq_pos != string::npos) {
        string var = args[0].substr(0, eq_pos);
        string val = args[0].substr(eq_pos + 1);
        if (setenv(var.c_str(), val.c_str(), 1) != 0) {
            cerr << "export: " << strerror(errno) << endl;
        }
    } else {
        cerr << "Incorrect format. Use VAR=value\n";
    }
}

void execute_cd(const vector<string>& args) {
    if (args.size() > 2) {
        cerr << "cd: Too many arguments\n";
    } else {
        const char* dir = (args.size() == 2) ? args[1].c_str() : getenv("HOME");
        if (chdir(dir) != 0) {
            cerr << "cd: " << strerror(errno) << endl;
        }
    }
}

// Handles I/O redirection in a given command
void handle_redirection(vector<string>& tokens) {
    auto it = find_redirection_operator(tokens);
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
vector<string>::iterator find_redirection_operator(vector<string>& tokens) {
    return find_if(tokens.begin(), tokens.end(),
                   [](const string& s) { return s == ">" || s == ">>" || s == "<"; });
}

// Execute an external command (not built-in)
void execute_external(const vector<string>& args) {
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Convert args to char* array for execvp
        char* argv[args.size() + 1];
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i] = const_cast<char*>(args[i].c_str());
        }
        argv[args.size()] = nullptr;

        execvp(argv[0], argv);
        cerr << "Command not found: " << argv[0] << endl;
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        wait(nullptr); // Wait for the child process to finish
    } else {
        cerr << "Failed to create a new process\n";
    }
}
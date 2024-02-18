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
void execute_command(vector<string> &args);

void execute_external(vector<string> &args);

vector<vector<string>> parse_command(const string &input, bool &is_parallel, bool &is_piped);

void handle_redirection(vector<string> &tokens);

vector<string>::iterator find_redirection_operator(vector<string> &tokens);

int is_env_assignment(const string &input);

void execute_cd(const vector<string> &args);

void execute_env_assignment(const vector<string> &args);

void execute_batch_commands(vector<vector<string>> &commands, bool is_parallel, bool is_piped);

void execute_parallel_commands(vector<vector<string>> &commands);

void execute_piped_commands(vector<vector<string>> &commands);

void pipe_execute(vector<string> &args);

int main(int argc, char **argv) {
    // Check command-line arguments to decide the mode of operation
    if (argc == 1) {
        while (true) {
            string input; // Input line

            cout << "Mish:" << getcwd(nullptr, 0) << "$ " << flush; // Print prompt
            getline(cin, input); // Read input line

            if (!input.empty()) { // Check if the input line is not empty
                bool is_parallel, is_piped;
                vector<vector<string>> commands = parse_command(input, is_parallel, is_piped); // Parse the input line
                execute_batch_commands(commands, is_parallel, is_piped); // Execute the command(s)
            }
        }
    } else if (argc == 2) {
        // Filename provided, run commands from the file (non-interactive mode)
        ifstream file(argv[1]);
        if (file.is_open()) {
            string line; // Line read from the file
            while (getline(file, line)) { // Read line by line from the file until EOF
                if (!line.empty()) { // Check if the line is not empty
                    bool is_parallel, is_piped;
                    vector<vector<string>> commands = parse_command(line, is_parallel,
                                                                    is_piped); // Parse the input line
                    execute_batch_commands(commands, is_parallel, is_piped); // Execute the command(s)
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

void execute_batch_commands(vector<vector<string>> &commands, bool is_parallel, bool is_piped) {
    if (is_parallel) {
        execute_parallel_commands(commands);
    } else if (is_piped) {
        execute_piped_commands(commands);
    } else {
        execute_command(commands[0]);
    }
}

void pipe_execute(vector<string> &args) {
    handle_redirection(args);

    char *argv[args.size() + 1];
    for (size_t i = 0; i < args.size(); ++i) {
        argv[i] = const_cast<char *>(args[i].c_str());
    }
    argv[args.size()] = nullptr;

    execvp(argv[0], argv); // Execute the command
    cerr << "Command not found: " << string(argv[0]) << endl << flush;
}

void execute_piped_commands(vector<vector<string>> &commands) {
    int num_commands = commands.size();
    vector<int> pids;
    int pipefd[2 * (num_commands - 1)]; // Array to hold the file descriptors for all pipes.

    // Create the pipes.
    for (int i = 0; i < num_commands - 1; ++i) {
        if (pipe(pipefd + i * 2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_commands; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // For all but the first command, redirect stdin from the previous pipe.
            if (i > 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }
            // For all but the last command, redirect stdout to the next pipe.
            if (i < num_commands - 1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors in the child.
            for (int j = 0; j < 2 * (num_commands - 1); ++j) {
                close(pipefd[j]);
            }

            pipe_execute(commands[i]);
            exit(EXIT_FAILURE); // Exit with failure if `execvp` returns.
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else {
            pids.push_back(pid);
        }
    }

    // Close all pipe file descriptors in the parent.
    for (int i = 0; i < 2 * (num_commands - 1); ++i) {
        close(pipefd[i]);
    }

    // Wait for all child processes to complete.
    int status;
    for (int pid: pids) {
        waitpid(pid, &status, 0);
    }
}

void execute_parallel_commands(vector<vector<string>> &commands) {
    vector<pid_t> child_pids;
    for (auto &args: commands) {
        pid_t pid = fork();
        if (pid == -1) {
            // Error handling.
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process.
            execute_command(args);
            exit(EXIT_SUCCESS); // Ensure child process exits after execution.
        } else {
            // Parent process.
            child_pids.push_back(pid);
        }
    }

    // Parent process waits for all child processes to complete.
    int status;
    for (pid_t pid: child_pids) {
        waitpid(pid, &status, 0);
    }
}

// Split the input line into command and arguments
vector<vector<string>> parse_command(const string &input, bool &is_parallel, bool &is_piped) {
    istringstream iss(input); // Create an input string stream
    vector<string> tokens; // Vector to store the tokens
    string token; // Temporary string to store each token
    vector<vector<string>> parallels; // Vector to store parallel commands separated by parallel operator
    vector<vector<string>> pipes; // Vector to store commands separated by pipe operator

    while (iss >> token) { // Read each token separated by whitespace
        // Check if the token contains '&' or '|'
        string tk = token;
        size_t pos = tk.find_first_of("&|");
        while (pos != string::npos) {
            if (pos == 0) { // If the operator is at the beginning of the token
                tokens.push_back(tk.substr(0, 1)); // Add the operator to the tokens
                tk = tk.substr(1); // Remove the operator from the token
            } else { // If the operator is in the middle
                tokens.push_back(tk.substr(0, pos)); // Add the command to the tokens
                tokens.push_back(tk.substr(pos, 1)); // Add the operator to the tokens
                tk = tk.substr(pos + 1); // Remove the command and operator from the token
            }
            pos = tk.find_first_of("&|"); // Find the next operator
        }
        if (!tk.empty()) {
            tokens.push_back(tk);
        }
    }

    if (tokens[0] == "exit") {
        // Check if there are any arguments to the exit command, if so print an error message and ignore the command
        if (tokens.size() > 1) { // If there are more than one argument
            cerr << "exit: Too many arguments\n";
            return parallels;
        } else {
            exit(0); // Exit the program
        }
    }

    // Loop over the tokens and separate the commands by the parallel operator
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "&") {
            parallels.push_back(vector<string>(tokens.begin(), tokens.begin() + i));
            tokens.erase(tokens.begin(), tokens.begin() + i + 1);
            i = 0;
        }
    }
    if (!tokens.empty()) {
        parallels.push_back(tokens);
    }
    // Check if parallels contains more than one command
    is_parallel = parallels.size() > 1;

    if (is_parallel) {
        is_piped = false;
        return parallels;
    } else {
        vector<string> parallel = parallels[0];
        for (size_t i = 0; i < parallel.size(); ++i) {
            if (parallel[i] == "|") {
                pipes.push_back(vector<string>(parallel.begin(), parallel.begin() + i));
                parallel.erase(parallel.begin(), parallel.begin() + i + 1);
                i = 0;
            }
        }
        if (!parallel.empty()) {
            pipes.push_back(parallel);
        }
        is_piped = pipes.size() > 1;
        is_parallel = false;
        return pipes;
    }
}

// Execute a command with its arguments
void execute_command(vector<string> &args) {
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
void execute_external(vector<string> &args) {
    pid_t pid = fork(); // Create a new process

    if (pid == 0) { // Child process
        handle_redirection(args);

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
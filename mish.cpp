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
#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

class Command {

public:
    vector<string> args; // Public member variable to store the string
    bool piped = false;
    bool parallel = false;

    // Constructor that takes a string and stores it in the data member
    explicit Command(const vector<string> &input, bool is_piped = false, bool is_parallel = false) {
        args = input;
        piped = is_piped;
        parallel = is_parallel;
    }
};

class Parser {
public:
    string data; // Public member variable to store the string
    vector<string> tokens; // Vector to store the tokens
    vector<Command> commands; // Vector to store the commands
    bool has_pipe = false;
    bool has_parallel = false;
    bool is_basic = false;
    bool is_valid = true;

    // Constructor that takes a string and stores it in the data member
    explicit Parser(const string &input) {
        data = input;
        preprocess();
        split();
        parse();
    }

    void preprocess() {
        string processed;
        for (char c : data) {
            if (c == '|' || c == '&') {
                processed += ' ';
                processed += c;
                processed += ' ';
            } else {
                processed += c;
            }
        }
        data = processed;
    }

    // Method to split the input line into tokens
    void split() {
        istringstream iss(data); // Create an input string stream
        string token; // Temporary string to store each token
        while (iss >> token) { // Read each token separated by whitespace
            tokens.push_back(token); // Add the token to the vector
            if (token == "|") { // Check if the token is a pipe operator
                has_pipe = true;
            } else if (token == "&") { // Check if the token is a parallel operator
                has_parallel = true;
            }
        }
        is_basic = !has_pipe && !has_parallel; // Check if the input line is a basic command
    }

    void parse() {
        if (is_basic) {
            commands.emplace_back(tokens);
        } else {
            vector<string> args;
            bool last_was_pipe = false;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "|") {
                    if (args.empty()) { // pipe operator at the beginning
                        is_valid = false;
                        cerr << "Error: Missing command before pipe operator\n";
                        return;
                    } else if (i == tokens.size() - 1) { // pipe operator at the end
                        is_valid = false;
                        cerr << "Error: Missing command after pipe operator\n";
                        return;
                    } else { // pipe operator in the middle
                        commands.emplace_back(args, true, false);
                        last_was_pipe = true;
                        args.clear(); // clear the args vector
                    }
                } else if (tokens[i] == "&") { // parallel operator
                    if (args.empty()) { // parallel operator at the beginning
                        is_valid = false;
                        cerr << "Error: Missing command before parallel operator\n";
                        return;
                    } else if ((i != tokens.size() - 1) && (tokens[i + 1] == "|")) { // parallel pipe
                        commands.emplace_back(args, true, true);
                        last_was_pipe = true;
                        args.clear(); // clear the args vector
                        i += 1; // skip the next token
                    } else { // parallel operator at the end
                        commands.emplace_back(args, last_was_pipe, true);
                        args.clear(); // clear the args vector
                    }
                } else {
                    args.push_back(tokens[i]);
                    if (i == tokens.size() - 1) { // last command
                        commands.emplace_back(args, last_was_pipe, false);
                    }
                }
            }
        }
    }

    int get_pipe_count() {
        int count = 0;
        for (auto &command: commands) {
            if (command.piped) {
                count += 1;
            }
        }
        return count;
    }
};


// Helper functions declarations
vector<string>::iterator find_redirection_operator(vector<string> &tokens);

bool handle_redirection(vector<string> &tokens);

int is_env_assignment(const string &input);

void execute_all_commands(Parser &parser);

void execute_cd(const vector<string> &args);

void execute_command(vector<string> &args);

void execute_env_assignment(const vector<string> &args);

void execute_external(vector<string> &args);

void execute_from_parser(Parser &parser);

void safe_execute(vector<string> &args);


void safe_execute(vector<string> &args) {
    if (!handle_redirection(args)) {
        exit(EXIT_FAILURE);
    }

    char *argv[args.size() + 1];
    for (size_t i = 0; i < args.size(); ++i) {
        argv[i] = const_cast<char *>(args[i].c_str());
    }
    argv[args.size()] = nullptr;

    execvp(argv[0], argv); // Execute the command
    cerr << "Command not found: " << string(argv[0]) << endl << flush;
}

void execute_from_parser(Parser &parser) {
    vector<pid_t> child_pids;
    int num_commands = parser.get_pipe_count() - 1;
    int pipefd[abs(2 * num_commands)];
    int count = -1;

    // Create the pipes.
    for (int i = 0; i < num_commands; ++i) {
        if (pipe(pipefd + i * 2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (auto &command: parser.commands) { // Loop over the commands
        if (command.piped) { count += 1; } // Increment the count for each piped command
        pid_t pid = fork(); // Create a new process
        if (pid == 0) {
            if (command.piped) {
                if (count > 0) { // For all but the first command, redirect stdin from the previous pipe
                    dup2(pipefd[(count - 1) * 2], STDIN_FILENO);
                }
                if (count < num_commands) { // For all but the last command, redirect stdout to the next pipe
                    dup2(pipefd[2 * count + 1], STDOUT_FILENO);
                }
                for (int i = 0; i < 2 * num_commands; ++i) {
                    close(pipefd[i]);
                }
            }
            safe_execute(command.args);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (!command.parallel) { // Parent process: check if the command is not parallel
            child_pids.push_back(pid);
        }
    }

    // Close all pipe file descriptors in the parent.
    for (int i = 0; i < 2 * num_commands; ++i) {
        close(pipefd[i]);
    }

    // Wait for all non-parallel child processes to complete.
    int status;
    for (int pid: child_pids) {
        waitpid(pid, &status, 0);
    }
}

void execute_all_commands(Parser &parser) {
    if (!parser.is_valid) { return; } // If the input line is not valid, return
    if (parser.is_basic) { // If the input line is a basic command
        execute_command(parser.commands[0].args);
    } else {
        execute_from_parser(parser);
    }
}

int is_env_assignment(const string &input) {
    //check if the command is an environment variable assignment
    regex pattern("^.+?=.+?$"); // Match any string followed by an equal sign and another string
    return regex_match(input, pattern);
}

void execute_command(vector<string> &args) {
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
    string path; // Path to change to
    if (args.size() > 2) {
        // If there are more than one argument (excluding the command itself), it's an error.
        cerr << "cd: Too many arguments" << endl;
        return;
    } else if (args.size() == 1) {
        cerr << "cd: Missing argument" << endl;
        return;
    } 

    if (args[1] == "..") {
        // change to the parent directory.
        path = "..";
    } else if (args[1] == "~") {
        // '~' is used, change to the home directory.
        char *home = getenv("HOME");
        if (home == nullptr) {
            cerr << "cd: HOME environment variable not set" << endl;
            return;
        }
        path = home;
    } else {
        // Use the provided argument as the path.
        path = args[1];
    }

    // Attempt to change the directory.
    if (chdir(path.c_str()) != 0) {
        // On failure, output the error.
        cerr << "cd: " << strerror(errno) << endl;
    }
}

bool handle_redirection(vector<string> &tokens) {
    auto it = find_redirection_operator(tokens); // Find the first redirection operator
    while (it != tokens.end()) {
        if (next(it) == tokens.end()) {
            cerr << "Redirection error: Missing filename\n";
            return false;
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
            return false;
        }

        if (op == "<") {
            dup2(fd, STDIN_FILENO);
        } else {
            dup2(fd, STDOUT_FILENO);
        }
        close(fd);

        it = find_redirection_operator(tokens); // Check if there are more redirections
    }
    return true;
}

vector<string>::iterator find_redirection_operator(vector<string> &tokens) {
    return find_if(tokens.begin(), tokens.end(),
                   [](const string &s) { return s == ">" || s == ">>" || s == "<"; });
}

void execute_external(vector<string> &args) {
    pid_t pid = fork(); // Create a new process

    if (pid == 0) { // Child process
        if (!handle_redirection(args)) {
            exit(EXIT_FAILURE);
        }

        char *argv[args.size() + 1];
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i] = const_cast<char *>(args[i].c_str());
        }
        argv[args.size()] = nullptr;

        execvp(argv[0], argv); // Execute the command
        cerr << "Command not found: " << string(argv[0]) << endl << flush;
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to finish
    } else {
        cerr << "Failed to create a new process\n";
    }
}

int main(int argc, char **argv) {
    // Initialize Readline's history
    using_history();

    if (argc == 1) { // No filename provided, run in interactive mode
        char *input;

        while (true) {
            string prompt = "Mish:" + string(getcwd(nullptr, 0)) + "$ ";
            input = readline(prompt.c_str()); // Read the input line with Readline

            if (input && *input) { // Check if the input line is not empty
                add_history(input); // Add the input to the history list

                string inputStr(input);
                Parser parser(inputStr);
                execute_all_commands(parser);
            }

            free(input); // Free the input line
        }
    } else if (argc == 2) { // Filename provided, run commands from the file (non-interactive mode)
        ifstream file(argv[1]);
        if (file.is_open()) {
            string line; // Line read from the file
            while (getline(file, line)) { // Read line by line from the file until EOF
                if (!line.empty()) { // Check if the line is not empty
                    Parser parser(line);
                    execute_all_commands(parser);
                }
            }
            file.close(); // Close the file
        } else { // Failed to open the file
            cerr << "Failed to open file: " << strerror(errno) << endl << flush; // Print error message
            exit(EXIT_FAILURE); // Exit with an error
        }
    } else { // More than one argument provided
        cerr << "Usage: " << argv[0] << " [script file]" << endl << flush; // Print usage
        exit(EXIT_FAILURE); // Exit with an error
    }
}
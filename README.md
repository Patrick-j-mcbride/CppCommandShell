
# Mish: A Simple Command Shell

Welcome to the official repository of **Mish**, a lightweight, simple command shell designed to offer a straightforward yet powerful command-line experience on Linux and macOS systems. 

#### Author : Patrick McBride
## Features

- **Two Modes of Operation**: Run interactively in user mode or execute commands from a `.sh` script file.
- **Command History**: Navigate through previously entered commands using the up and down arrow keys.
- **Cursor Movement**: Move the cursor through the current line with left and right arrows.
- **Built-in Commands**:
  - `exit`: Exits the program.
  - `cd`: Changes the current directory.
  - `VAR=val`: Sets the environment variable `VAR` to `val`.
  - `export VAR=val`: Exports the environment variable `VAR` with the value `val`.
  - `echo $VAR`: Outputs the value of the environment variable `VAR`.
- **Input and Output Redirectors**:
  - `>`: Redirects the output to a file, overwriting it.
  - `>>`: Appends the output to a file.
  - `<`: Takes input from a file.
- **Pipelines**: Allows the output of one command to be used as input for another using the `|` symbol.
- **Parallel Commands**: Execute commands in parallel with the shell using `&`.
- **Advanced Pipeline and Parallel Execution**: Combines pipelines and parallel execution for complex command orchestration.

## Prerequisites

Before compiling Mish, ensure you have the GNU Readline library installed on your system. This library is essential for Mish to operate correctly.

## Compiling Mish

1. **Install GNU Readline Library**:
   - On macOS: `brew install readline`

2. **Compile Mish**:
   - Navigate to the Mish source directory.
   - Run `make` to compile the shell. This will generate the `mish` executable.
   - It should find your readline and compile with the following:
     - g++ -g -Wall -std=c++14 -Werror -O -c mish.cpp -o mish.o
     - g++ -g -Wall -std=c++14 -Werror -O -I -L -lreadline -o Mish mish.o

## Usage Instructions

### Starting Mish

- **User Mode**: Simply run `./mish` without any arguments to start Mish in user mode.
- **File Mode**: To execute commands from a file, run `./mish filename.sh`, where `filename.sh` is the path to your script file.

### Working with Commands

- **Navigating Command History**: Use the up and down arrows to scroll through your command history.
- **Cursor Movement**: Use the left and right arrows to move the cursor through the command line.

### Built-in Commands

- **Exiting Mish**: Type `exit` and press Enter.
- **Changing Directories**: Use `cd` followed by the path to change directories.
- **Setting Environment Variables**:
  - Direct assignment: `VAR=value`
  - Using `export`: `export VAR=value`
- **Echoing Variables**: Use `echo $VAR` to display the value of `VAR`.

### Using Redirectors and Pipelines

- **Redirecting Output**: Use `command > file` to overwrite a file or `command >> file` to append.
- **Input Redirection**: Use `command < file` to feed the content of `file` into `command`.
- **Creating Pipelines**: Chain commands with `|`, like `cmd1 | cmd2 | cmd3 > result`, to pass data through multiple processes.
- **Parallel Execution**: Append `&` to run commands in parallel. Use `cmd1 & | cmd2 &` for full parallel execution or `cmd1 & | cmd2` for partial.

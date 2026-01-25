# Project 1a 

This project implements a simple shell that reads user input, execute
the specified command in a child process and waits for it to complete.

### Features
- Displays the `penn-shredder# ` prompt
- Reads input using `read()`
- Trims leading and trailing whitespace
- Ignores empty input
- Executes commands using `fork()` and `execve()`
- Handles Ctrl-D (EOF) correctly

### Notes
- Commands must be entered with full paths (e.g., `/bin/ls`)
- Timeout and alarm functionality not included in Part 1a (will be included in part 1b)

### Files
- `penn-shredder.c`
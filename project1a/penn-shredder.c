/* include these header files to use their functions */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Macro to universaly define the size of the input.
 *
 * The compiler will replace all instances of this macro with the value.
 *
 * This helps avoid "magic numbers", which you should avoid in your code.
 */
#define INPUT_SIZE 1024

/* This is declared outside of main, so it is a global variable.
 *
 * This is the ONLY global variable you are allowed to use.
 *
 * You must ensure that this value is updated when appropriate.
 *
 * In general, avoid the use of global variables in any code you write.
 */
pid_t childPid = 0;

/* In C, you must declare the function before
 * it is used elsewhere in your program.
 *
 * By defining them at the top of the program, you avoid
 * implied declaration warnings (the compiler will guess that the return value is an int).
 *
 * This is normally implemented as a header (.h) file.
 *
 * You may choose to refactor this into a header file,
 * as long as you update your makefile orrectly.
 */
void alarmHandler(int sig);
void executeShell(int timeout);
char *getCommandFromInput();
void killChildProcess();
void registerSignalHandlers();
void sigintHandler(int sig);
void writeToStdout(char *text);

/* This is the main function.
 *
 * It will register the signal handlers via function call,
 * check for a timeout (for project1b),
 * and call `executeShell` in a loop indefinitely.
 *
 * DO NOT modify this function, it is correctly implemented for you.
 */
int main(int argc, char **argv) {
    registerSignalHandlers();

    int timeout = 0;
    if (argc == 2) {
        timeout = atoi(argv[1]);
    }

    if (timeout < 0) {
        writeToStdout("Invalid input detected. Ignoring timeout value.\n");
        timeout = 0;
    }

    while (1) {
        executeShell(timeout);
    }

    return 0;
}

/* Sends SIGKILL signal to the child process identified by `childPid`.
 *
 * It will check for system call errors and exit penn-shredder program if
 * there is an error.
 *
 * DO NOT modify this function, it is correctly implemented for you.
 */
void killChildProcess() {
    if (kill(childPid, SIGKILL) == -1) {
        perror("Error in kill");
        exit(EXIT_FAILURE);
    }
}

/* Signal handler for SIGALRM.
 *
 * It should kill the child process.
 *
 * It should then print out penn-shredder's catchphrase to standard output.
 *
 * If no child process currently exists, it should take no action.
 *
 * TODO: implement in project1b.
 */
void alarmHandler(int sig) {
}

/* Signal handler for SIGINT.
 *
 * Kills the child process if childPid is non-zero.
 *
 * Takes no action if the child process does not exist.
 *
 * Takes no action on  parent process and its execution.
 *
 * When a user enters Control+C, this sends the SIGINT signal to the program.
 * If this function is registered to run on SIGINT, 
 * it will run the function body instead of the default behaviour.
 * Read the manual pages for signal (section 7) to see the default behaviour of all signals.
 *
 * DO NOT modify this function, it is correctly implemented for you.
 */
void sigintHandler(int sig) {
    if (childPid != 0) {
        killChildProcess();
    }
}


/* Registers SIGALRM and SIGINT handlers with corresponding functions.
 *
 * Checks for system call failure and exits program if
 * there is an error.
 *
 * TODO: implement the SIGARLM portion in project1b.
 *
 * The SIGINT portion is implemented correctly for you.
 */
void registerSignalHandlers() {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }
}

/* Prints the shell prompt and waits for input from user.
 *
 * It then creates a child process which executes the command.
 *
 * The parent process waits for the child and checks that
 * it was either signalled or exited.
 *
 * TODO: you may modify this function for project1a and project1b.
 *
 * TODO: implement the alarm portion in project1b.
 * Use the timeout argument to start an alarm of that timeout period.
 * */
void executeShell(int timeout) {
    char *command;
    int status;
    char minishell[] = "penn-shredder# ";
    writeToStdout(minishell);

    command = getCommandFromInput();

    if (command != NULL) {
        childPid = fork();

        if (childPid < 0) {
            perror("Error in creating child process");
            exit(EXIT_FAILURE);
        }

        if (childPid == 0) {
            char *const envVariables[] = {NULL};
            char *const args[] = {command, NULL};
            if (execve(command, args, envVariables) == -1) {
                perror("Error in execve");
                exit(EXIT_FAILURE);
            }
        } else {
            do {
                if (wait(&status) == -1) {
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
}

/* Writes particular text to standard output.
 *
 * Exits penn-shredder if the system call `write` fails.
 *
 * DO NOT modify this function, it is correctly implemnted for you.
 */
void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        perror("Error in write");
        exit(EXIT_FAILURE);
    }
}

/* Reads input from standard input using `read` system call.
 * You are not permitted to use other functions like `fgets`.
 *
 * If the user enters Control+D from the keyboard,
 * penn-shredder exits immediately.
 *
 * If the user enters text followed by Control+D,
 * penn-shredder does not exit and does not report an error.
 *
 * Note specifically that Control+D represents End-Of-File (EOF).
 * This is not a character that can be read.  It just tells `read` that
 * there is no more data to read from the keyboard.
 *
 * The leading and trailing spaces must be removed from the user input.
 *
 * The string must be NULL-terminated.
 *
 * Note that the starter code is hardcoded to return "/bin/ls",
 * which will cause an infinite loop as provided.
 *
 * TODO: implement this function for project1a.
 */

char *getCommandFromInput() {
    char buf[INPUT_SIZE + 1];
    memset(buf, 0, sizeof(buf));

    int n = (int)read(STDIN_FILENO, buf, INPUT_SIZE);
    if (n < 0) {
        perror("invalid read");
        exit(EXIT_FAILURE);
    }

    /* Ctrl-D with no text, then EOF immediately */
    if (n == 0) {
        exit(0);
    }

    buf[n] = '\0';

    /* Strip trailing newline if present */
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
        n--;
    }

    /* Trim leading spaces */
    int start = 0;
    while (buf[start] == ' ' || buf[start] == '\t') {
        start++;
    }

    /* Trim trailing spaces */
    int end = n - 1;
    while (end >= start && (buf[end] == ' ' || buf[end] == '\t')) {
        buf[end] = '\0';
        end--;
    }

    /* If empty or only spaces */
    if (end < start) {
        return NULL;
    }

    int len = end - start + 1;

    char *cmd = (char *)malloc((size_t)len + 1);
    if (cmd == NULL) {
        perror("invalid malloc");
        exit(EXIT_FAILURE);
    }

    memset(cmd, 0, (size_t)len + 1);
    strncpy(cmd, buf + start, (size_t)len);
    cmd[len] = '\0';

    return cmd;
}

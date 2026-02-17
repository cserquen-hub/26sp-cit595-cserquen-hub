#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "tokenizer.h"

#define INPUT_SIZE 1024
#define MAX_TOKENS 512
#define MAX_ARGS   256
#define MAX_CMDS   64

pid_t childPid = 0;


void alarmHandler(int sig);
void executeShell(int timeout);
char *getCommandFromInput();
void killChildProcess();
void registerSignalHandlers();
void sigintHandler(int sig);
void writeToStdout(char *text);



/* ------------ main ------------- */

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


/* ---------------- signals ------------- */

void killChildProcess() {
    if (kill(childPid, SIGKILL) == -1) {
        perror("Error in kill");
        exit(EXIT_FAILURE);
    }
}

void alarmHandler(int sig) {
    if (sig == SIGALRM) {
        if (childPid != 0) {
            killChildProcess();
            writeToStdout("Bwahaha ... tonight I dine on turtle soup\n");
        }
    }
}


void sigintHandler(int sig) {
    (void)sig;
    if (childPid != 0) {
        killChildProcess();
    }
}

void registerSignalHandlers() {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }
}

void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        perror("Error in write");
        exit(EXIT_FAILURE);
    }
}


/* ---------------- helpers ---------------- */

static void free_tokens(char **tokens, int ntok) {
    for (int i = 0; i < ntok; i++) free(tokens[i]);
}

static int is_op(const char *t) {
    return t &&
           (!strcmp(t, "<") || !strcmp(t, ">") || !strcmp(t, "|"));
}



/* ---------------- executeShell ---------------- */

void executeShell(int timeout) {
    char minishell[] = "penn-shredder# ";
    writeToStdout(minishell);

    alarm(0);

    char *command = getCommandFromInput();
    if (command == NULL) return;

    TOKENIZER *t = init_tokenizer(command);
    if (t == NULL) {
        writeToStdout("invalid\n");
        free(command);
        return;
    }

    char *tokens[MAX_TOKENS];
    int ntok = 0;

    char *tok;
    while ((tok = get_next_token(t)) != NULL) {
        if (ntok >= MAX_TOKENS) {
            free(tok);
            writeToStdout("invalid\n");
            free_tokenizer(t);
            free(command);
            return;
        }
        tokens[ntok++] = tok;
    }
    free_tokenizer(t);

    if (ntok == 0) {
        free(command);
        return;
    }

 
    if (is_op(tokens[0]) || is_op(tokens[ntok - 1])) {
        writeToStdout("invalid\n");
        free_tokens(tokens, ntok);
        free(command);
        return;
    }

    for (int i = 0; i < ntok - 1; i++) {
        if (is_op(tokens[i]) && is_op(tokens[i + 1])) {
            writeToStdout("invalid\n");
            free_tokens(tokens, ntok);
            free(command);
            return;
        }
    }

    /* find pipes */
    int pipe_pos[MAX_CMDS];
    int num_pipes = 0;

    for (int i = 0; i < ntok; i++) {
        if (!strcmp(tokens[i], "|")) {
            pipe_pos[num_pipes++] = i;
        }
    }

    int num_cmds = num_pipes + 1;

    int pipefds[2 * MAX_CMDS];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            free_tokens(tokens, ntok);
            free(command);
            return;
        }
    }

    pid_t pids[MAX_CMDS];
    int start = 0;

    for (int c = 0; c < num_cmds; c++) {
        int end = (c < num_pipes) ? pipe_pos[c] : ntok;

        pid_t pid = fork();

        if (pid == 0) {
            /* child */

            if (c > 0) dup2(pipefds[(c - 1) * 2], STDIN_FILENO);
            if (c < num_cmds - 1) dup2(pipefds[c * 2 + 1], STDOUT_FILENO);

            for (int i = 0; i < 2 * num_pipes; i++) close(pipefds[i]);

            char *args[MAX_ARGS];
            int argc = 0;

            int in_count = 0, out_count = 0;
            char *infile = NULL, *outfile = NULL;

            for (int i = start; i < end; i++) {
                if (!strcmp(tokens[i], "<")) {
                    in_count++;
                    if (i + 1 >= end || is_op(tokens[i + 1])) {
                        writeToStdout("invalid\n");
                        _exit(1);
                    }
                    infile = tokens[++i];
                }
                else if (!strcmp(tokens[i], ">")) {
                    out_count++;
                    if (i + 1 >= end || is_op(tokens[i + 1])) {
                        writeToStdout("invalid\n");
                        _exit(1);
                    }
                    outfile = tokens[++i];
                }
                else {
                    args[argc++] = tokens[i];
                }
            }

            args[argc] = NULL;

            if (argc == 0 || in_count > 1 || out_count > 1) {
                writeToStdout("invalid\n");
                _exit(1);
            }

            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) _exit(1);
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) _exit(1);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(args[0], args);
            writeToStdout("invalid\n");
            _exit(1);
        }

        pids[c] = pid;
        childPid = pid;
        start = end + 1;
    }

    for (int i = 0; i < 2 * num_pipes; i++) close(pipefds[i]);

    if (timeout > 0) alarm(timeout);

    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }

    alarm(0);
    childPid = 0;

    free_tokens(tokens, ntok);
    free(command);
}

/* ---------------- getcommandFromInput --------- */

char *getCommandFromInput() {
    char buf[INPUT_SIZE + 1];
    memset(buf, 0, sizeof(buf));

    int n = (int)read(STDIN_FILENO, buf, INPUT_SIZE);
    if (n < 0) {
        perror("invalid read");
        exit(EXIT_FAILURE);
    }

    if (n == 0) {
        exit(0);
    }

    buf[n] = '\0';

    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
        n--;
    }

    int start = 0;
    while (buf[start] == ' ' || buf[start] == '\t') start++;

    int end = n - 1;
    while (end >= start && (buf[end] == ' ' || buf[end] == '\t')) {
        buf[end] = '\0';
        end--;
    }

    if (end < start) return NULL;

    int len = end - start + 1;

    char *cmd = malloc(len + 1);
    if (!cmd) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strncpy(cmd, buf + start, len);
    cmd[len] = '\0';

    return cmd;
}




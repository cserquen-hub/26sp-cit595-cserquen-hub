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


static pid_t childPid = 0;


static void alarmHandler(int sig);
static void sigintHandler(int sig);
static void registerSignalHandlers(void);
static void writeToStdout(const char *text);
static char *getCommandFromInput(void);

static void free_tokens(char **tokens, int ntok);
static int is_op(const char *t);
static int parse_segment(char **tokens, int start, int end,
                         char **argv_out, int *argc_out,
                         char **infile_out, char **outfile_out);

void executeShell(int timeout);


/* -------------- main ---------------- */

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


/* ---------------- signals --------------  */

static void killChildProcessGroup(void) {
    if (childPid != 0) {
        /* negative pid => kill process group */
        if (kill(-childPid, SIGKILL) == -1) {
            /* don't exit the shell on kill failure */
            /* (perror is not async-signal-safe; avoid here) */
        }
    }
}


static void alarmHandler(int sig) {
    if (sig == SIGALRM) {
        if (childPid != 0) {
            killChildProcessGroup();
            /* write is async-signal-safe */
            writeToStdout("Bwahaha ... tonight I dine on turtle soup\n");
        }
    }
}

static void sigintHandler(int sig) {
    (void)sig;
    if (childPid != 0) {
        killChildProcessGroup();
    }
}

static void registerSignalHandlers(void) {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

/* -------- -------- stdout helper ---------------- */

static void writeToStdout(const char *text) {
    /* safe for signals if called with constant strings */
    size_t len = strlen(text);
    if (write(STDOUT_FILENO, text, len) == -1) {
        /* if stdout breaks, just exit */
        _exit(EXIT_FAILURE);
    }
}

/* ---------------- tokenizer helpers ---------------- */

static void free_tokens(char **tokens, int ntok) {
    for (int i = 0; i < ntok; i++) {
        free(tokens[i]);
    }
}

static int is_op(const char *t) {
    return (t != NULL) &&
           (strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, "|") == 0);
}



static int parse_segment(char **tokens, int start, int end,
                         char **argv_out, int *argc_out,
                         char **infile_out, char **outfile_out)
{
    int argc = 0;
    int in_count = 0, out_count = 0;
    char *infile = NULL, *outfile = NULL;

    for (int i = start; i < end; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            in_count++;
            if (i + 1 >= end || is_op(tokens[i + 1])) return 0;
            infile = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0) {
            out_count++;
            if (i + 1 >= end || is_op(tokens[i + 1])) return 0;
            outfile = tokens[++i];
        } else {
            if (argc >= MAX_ARGS - 1) return 0;
            argv_out[argc++] = tokens[i];
        }
    }

    if (argc == 0) return 0;
    if (in_count > 1 || out_count > 1) return 0;

    argv_out[argc] = NULL;
    *argc_out = argc;
    *infile_out = infile;
    *outfile_out = outfile;
    return 1;
}

/* ---------------- getcommandFromInput ---------------- */

static char *getCommandFromInput(void) {
    char buf[INPUT_SIZE + 1];
    memset(buf, 0, sizeof(buf));

    int n = (int)read(STDIN_FILENO, buf, INPUT_SIZE);
    if (n < 0) {
        perror("read");
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
    char *cmd = (char *)malloc((size_t)len + 1);
    if (cmd == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strncpy(cmd, buf + start, (size_t)len);
    cmd[len] = '\0';
    return cmd;
}

/* ---------------- 2B  ---------------- */

void executeShell(int timeout) {
    writeToStdout("penn-shredder# ");

    /* cancel any previous alarm */
    alarm(0);

    char *command = getCommandFromInput();
    if (command == NULL) {
        return;
    }

    /* tokenize */
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


    int pipe_index = -1;
    for (int i = 0; i < ntok; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            if (pipe_index != -1) {
                writeToStdout("invalid\n");
                free_tokens(tokens, ntok);
                free(command);
                return;
            }
            pipe_index = i;
        }
    }


    if (pipe_index == -1) {
        char *argv[MAX_ARGS];
        int argc = 0;
        char *infile = NULL, *outfile = NULL;

        if (!parse_segment(tokens, 0, ntok, argv, &argc, &infile, &outfile)) {
            writeToStdout("invalid\n");
            free_tokens(tokens, ntok);
            free(command);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            writeToStdout("invalid\n");
            free_tokens(tokens, ntok);
            free(command);
            return;
        }

        if (pid == 0) {
    
            setpgid(0, 0);

            if (infile != NULL) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { writeToStdout("invalid\n"); _exit(1); }
                if (dup2(fd, STDIN_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }
                close(fd);
            }

            if (outfile != NULL) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { writeToStdout("invalid\n"); _exit(1); }
                if (dup2(fd, STDOUT_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }
                close(fd);
            }

            execvp(argv[0], argv);
            writeToStdout("invalid\n");
            _exit(1);
        }


        setpgid(pid, pid);
        childPid = pid;

        if (timeout > 0) alarm((unsigned int)timeout);

        waitpid(pid, NULL, 0);

        alarm(0);
        childPid = 0;

        free_tokens(tokens, ntok);
        free(command);
        return;
    }


    char *largv[MAX_ARGS], *rargv[MAX_ARGS];
    int largc = 0, rargc = 0;
    char *linfile = NULL, *loutfile = NULL;
    char *rinfile = NULL, *routfile = NULL;

    if (!parse_segment(tokens, 0, pipe_index, largv, &largc, &linfile, &loutfile) ||
        !parse_segment(tokens, pipe_index + 1, ntok, rargv, &rargc, &rinfile, &routfile)) {
        writeToStdout("invalid\n");
        free_tokens(tokens, ntok);
        free(command);
        return;
    }


    if (loutfile != NULL || rinfile != NULL) {
        writeToStdout("invalid\n");
        free_tokens(tokens, ntok);
        free(command);
        return;
    }

    int pfds[2];
    if (pipe(pfds) < 0) {
        perror("pipe");
        writeToStdout("invalid\n");
        free_tokens(tokens, ntok);
        free(command);
        return;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork");
        writeToStdout("invalid\n");
        close(pfds[0]); close(pfds[1]);
        free_tokens(tokens, ntok);
        free(command);
        return;
    }

    if (left_pid == 0) {

        setpgid(0, 0);

        if (dup2(pfds[1], STDOUT_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }

        if (linfile != NULL) {
            int fd = open(linfile, O_RDONLY);
            if (fd < 0) { writeToStdout("invalid\n"); _exit(1); }
            if (dup2(fd, STDIN_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }
            close(fd);
        }

        close(pfds[0]);
        close(pfds[1]);

        execvp(largv[0], largv);
        writeToStdout("invalid\n");
        _exit(1);
    }

    /* parent: make sure left is PG leader */
    setpgid(left_pid, left_pid);
    childPid = left_pid; 

    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork");
        writeToStdout("invalid\n");
        close(pfds[0]); close(pfds[1]);
        waitpid(left_pid, NULL, 0);
        childPid = 0;
        free_tokens(tokens, ntok);
        free(command);
        return;
    }

    if (right_pid == 0) {

        setpgid(0, left_pid);

        if (dup2(pfds[0], STDIN_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }

        if (routfile != NULL) {
            int fd = open(routfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { writeToStdout("invalid\n"); _exit(1); }
            if (dup2(fd, STDOUT_FILENO) < 0) { writeToStdout("invalid\n"); _exit(1); }
            close(fd);
        }

        close(pfds[0]);
        close(pfds[1]);

        execvp(rargv[0], rargv);
        writeToStdout("invalid\n");
        _exit(1);
    }


    close(pfds[0]);
    close(pfds[1]);

    if (timeout > 0) alarm((unsigned int)timeout);

    /* wait both children */
    waitpid(left_pid, NULL, 0);
    waitpid(right_pid, NULL, 0);

    alarm(0);
    childPid = 0;

    free_tokens(tokens, ntok);
    free(command);
}
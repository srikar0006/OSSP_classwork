/*
 * command_executor.c
 *
 * Demonstrates how a Linux shell executes a user command:
 *   1. Accept a command as input
 *   2. fork() a child process
 *   3. exec() the command in the child
 *   4. Parent wait()s for the child
 *   5. Display PIDs of both parent and child
 *
 */

#include <stdio.h>      // printf, fgets
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strtok, strlen
#include <unistd.h>     // fork, execvp, getpid, getppid
#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // wait, WIFEXITED, WEXITSTATUS

#define MAX_INPUT   1024
#define MAX_ARGS    64

/*
 * Splits the raw input line into an argv[]-style array
 * suitable for execvp(). Example:
 *   "ls -l /home"  ->  {"ls", "-l", "/home", NULL}
 */
int parse_command(char *input, char *args[]) {
    int argc = 0;

    char *token = strtok(input, " \t\n");
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;   // execvp requires a NULL-terminated array
    return argc;
}

int main(void) {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    printf("=== Linux Command Execution Demo (fork + exec + wait) ===\n");
    printf("Parent process started. Parent PID = %d\n", getpid());

    while (1) {
        printf("\nmyshell> ");
        fflush(stdout);

        // Step 1: Accept a command as input
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\nEOF received. Exiting.\n");
            break;
        }

        // Strip trailing newline
        input[strcspn(input, "\n")] = '\0';

        // Skip empty input
        if (strlen(input) == 0) {
            continue;
        }

        // Allow user to quit the demo
        if (strcmp(input, "exit") == 0) {
            printf("Exiting shell demo.\n");
            break;
        }

        // Parse the input line into argv[] form
        int argc = parse_command(input, args);
        if (argc == 0) {
            continue;
        }

        // Step 2: Create a child process
        pid_t pid = fork();

        if (pid < 0) {
            // fork() failed
            perror("fork failed");
            continue;

        } else if (pid == 0) {
            // ---------- CHILD PROCESS ----------
            printf("[Child]  PID = %d, Parent PID = %d\n", getpid(), getppid());
            printf("[Child]  Executing command: %s\n\n", input);

            // Step 3: Execute the command using execvp()
            // execvp() replaces the child's memory image with the new program.
            // It searches PATH automatically (that's why we use the 'p' variant).
            execvp(args[0], args);

            // If execvp() returns at all, it means it FAILED
            fprintf(stderr, "execvp failed: command '%s' not found\n", args[0]);
            exit(EXIT_FAILURE);

        } else {
            // ---------- PARENT PROCESS ----------
            int status;
            printf("[Parent] PID = %d, Created Child PID = %d\n", getpid(), pid);

            // Step 4: Wait for the child to finish
            pid_t terminated_pid = wait(&status);

            // Step 5 (reporting): show exit status details
            if (WIFEXITED(status)) {
                printf("[Parent] Child (PID = %d) exited normally with status %d\n",
                       terminated_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[Parent] Child (PID = %d) was killed by signal %d\n",
                       terminated_pid, WTERMSIG(status));
            }
        }
    }

    return 0;
}

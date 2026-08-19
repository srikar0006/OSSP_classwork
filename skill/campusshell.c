/* ============================================================
 * CampusShell - Smart University Computing Lab Management System
 * ------------------------------------------------------------
 * A teaching-oriented custom Unix shell written in C.
 *
 * Demonstrates:
 *   - The shell as an interface between user and OS
 *   - Command parsing and tokenization
 *   - Process creation with fork()
 *   - Program replacement with execvp()
 *   - Synchronization with wait()/waitpid()
 *   - Foreground vs background execution ( & )
 *   - Built-in commands (cd, pwd, exit, help, history, jobs, clear)
 *   - Simple process monitoring (labps, sysinfo)
 *   - Signal handling (Ctrl+C should not kill the shell itself)
 *
 * Compile:
 *   gcc -Wall -o campusshell campusshell.c
 *
 * Run:
 *   ./campusshell
 *
 * Author: Generated for OS / Systems Programming course use
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <limits.h>
#include <pwd.h>

#define MAX_LINE      1024
#define MAX_ARGS      64
#define MAX_HISTORY   100
#define MAX_JOBS      50

/* ---------- Global state ---------- */

char *history[MAX_HISTORY];
int history_count = 0;

typedef struct {
    int   job_id;
    pid_t pid;
    char  command[MAX_LINE];
    int   running;   /* 1 = still running (as far as we know), 0 = finished */
} Job;

Job job_list[MAX_JOBS];
int job_count = 0;

/* ---------- Utility / UI functions ---------- */

void print_banner(void) {
    printf("=====================================================\n");
    printf("   CampusShell v1.0 - University Computing Lab Shell   \n");
    printf("   Type 'help' to see available commands              \n");
    printf("=====================================================\n");
}

void print_prompt(void) {
    char cwd[PATH_MAX];
    char hostname[256];
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "user";

    if (gethostname(hostname, sizeof(hostname)) != 0)
        strcpy(hostname, "lab-pc");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    printf("\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", username, hostname, cwd);
    fflush(stdout);
}

/* Add a raw command line to the history buffer */
void add_history(const char *line) {
    if (strlen(line) == 0) return;

    if (history_count < MAX_HISTORY) {
        history[history_count++] = strdup(line);
    } else {
        /* Shift history left, drop oldest */
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY; i++)
            history[i - 1] = history[i];
        history[MAX_HISTORY - 1] = strdup(line);
    }
}

/* ---------- Input reading & parsing ---------- */

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("\n");
            exit(EXIT_SUCCESS);   /* Ctrl+D pressed */
        } else {
            perror("campusshell: read error");
            exit(EXIT_FAILURE);
        }
    }

    /* Strip trailing newline */
    line[strcspn(line, "\n")] = '\0';
    return line;
}

/* Split a line into an argv-style array of tokens.
 * Returns the number of tokens found. */
int parse_line(char *line, char **args) {
    int count = 0;
    char *token = strtok(line, " \t");

    while (token != NULL && count < MAX_ARGS - 1) {
        args[count++] = token;
        token = strtok(NULL, " \t");
    }
    args[count] = NULL;
    return count;
}

/* ---------- Built-in commands ---------- */
void builtin_help(void) {
    printf("CampusShell - Built-in commands:\n");
    printf("  cd <dir>      Change current directory\n");
    printf("  pwd           Print current working directory\n");
    printf("  history       Show command history\n");
    printf("  jobs          List background jobs started in this shell\n");
    printf("  labps         Show snapshot of running processes (via ps)\n");
    printf("  sysinfo       Show basic CPU/memory info for the lab machine\n");
    printf("  clear         Clear the terminal screen\n");
    printf("  exit          Exit CampusShell\n");
    printf("Any other input is treated as an external Linux command\n");
    printf("(e.g., ls, gcc, cat). Append '&' to run a command in background.\n");
}

void builtin_cd(char **args) {
    if (args[1] == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && chdir(pw->pw_dir) != 0)
            perror("campusshell: cd");
    } else {
        if (chdir(args[1]) != 0)
            perror("campusshell: cd");
    }
}

void builtin_history(void) {
    for (int i = 0; i < history_count; i++)
        printf("%4d  %s\n", i + 1, history[i]);
}

void builtin_jobs(void) {
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        int status;
        pid_t res = waitpid(job_list[i].pid, &status, WNOHANG);
        if (res == 0) {
            job_list[i].running = 1;
        } else if (res == job_list[i].pid) {
            job_list[i].running = 0;
        }
        printf("[%d] %s  PID:%d  %s\n",
               job_list[i].job_id,
               job_list[i].running ? "Running" : "Done   ",
               job_list[i].pid,
               job_list[i].command);
    }
}

/* Snapshot of active processes - useful for a "lab monitoring" feel */
void builtin_labps(void) {
    printf("---- CampusShell Lab Process Monitor ----\n");
    system("ps -eo pid,ppid,user,%cpu,%mem,cmd --sort=-%cpu | head -n 15");
}

/* Very small system info panel (CPU count, load avg, memory) */
void builtin_sysinfo(void) {
    printf("---- Lab Machine System Info ----\n");
    system("echo -n 'CPU cores : '; nproc");
    system("echo -n 'Load avg  : '; cat /proc/loadavg | awk '{print $1, $2, $3}'");
    system("free -h | awk 'NR==1 || NR==2 {print}'");
}

/* ---------- Command execution ---------- */

/* Executes an external (non-builtin) program.
 * background = 1 if the command ended with '&' */
void execute_external(char **args, int background, const char *raw_line) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("campusshell: fork failed");
        return;
    }

    if (pid == 0) {
        /* Child process: restore default Ctrl+C behavior, then exec */
        signal(SIGINT, SIG_DFL);
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "campusshell: command not found: %s\n", args[0]);
            exit(127);
        }
    } else {
        /* Parent process */
        if (background) {
            if (job_count < MAX_JOBS) {
                job_list[job_count].job_id = job_count + 1;
                job_list[job_count].pid = pid;
                strncpy(job_list[job_count].command, raw_line, MAX_LINE - 1);
                job_list[job_count].running = 1;
                printf("[%d] started in background, PID %d\n",
                       job_list[job_count].job_id, pid);
                job_count++;
            }
            /* Do not block the shell prompt for background jobs */
        } else {
            int status;
            waitpid(pid, &status, 0);  /* Wait for foreground child to finish */
        }
    }
}

/* Returns 1 if the built-in was handled (including exit), 0 otherwise */
int execute_builtin(char **args, int argc, int *should_exit) {
    if (argc == 0) return 1; /* empty line, nothing to do */
if (strcmp(args[0], "exit") == 0) {
        printf("Exiting CampusShell. Goodbye!\n");
        *should_exit = 1;
        return 1;
    }
    if (strcmp(args[0], "cd") == 0) {
        builtin_cd(args);
        return 1;
    }
    if (strcmp(args[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
        return 1;
    }
    if (strcmp(args[0], "help") == 0) {
        builtin_help();
        return 1;
    }
    if (strcmp(args[0], "history") == 0) {
        builtin_history();
        return 1;
    }
    if (strcmp(args[0], "jobs") == 0) {
        builtin_jobs();
        return 1;
    }
    if (strcmp(args[0], "labps") == 0) {
        builtin_labps();
        return 1;
    }
    if (strcmp(args[0], "sysinfo") == 0) {
        builtin_sysinfo();
        return 1;
    }
    if (strcmp(args[0], "clear") == 0) {
        system("clear");
        return 1;
    }

    return 0; /* not a builtin */
}

/* ---------- Signal handling ---------- */

void sigint_handler(int sig) {
    (void)sig;
    /* Just print a fresh prompt line; the shell itself keeps running.
     * Any foreground child gets its own default SIGINT via execute_external. */
    printf("\n");
    print_prompt();
}

/* ---------- Main loop ---------- */

int main(void) {
    char *line;
    char *args[MAX_ARGS];
    int argc;
    int should_exit = 0;

    signal(SIGINT, sigint_handler);

    print_banner();

    while (!should_exit) {
        print_prompt();
        line = read_line();

        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        add_history(line);

        /* Keep a copy of the raw line for job listing, since strtok
         * will destroy 'line' during parsing. */
        char raw_copy[MAX_LINE];
        strncpy(raw_copy, line, MAX_LINE - 1);
        raw_copy[MAX_LINE - 1] = '\0';

        /* Detect trailing '&' for background execution */
        int background = 0;
        char *amp = strrchr(line, '&');
        if (amp != NULL && *(amp + 1) == '\0') {
            background = 1;
            *amp = '\0'; /* remove the & before tokenizing */
        }

        argc = parse_line(line, args);

        if (!execute_builtin(args, argc, &should_exit)) {
            if (argc > 0) {
                execute_external(args, background, raw_copy);
            }
        }

        free(line);
    }

    /* Free history memory */
    for (int i = 0; i < history_count; i++)
        free(history[i]);

    return 0;
}

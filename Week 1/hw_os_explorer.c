/*
 *
 * Investigates the relationship between hardware resources and OS services
 * by using fork() + execvp() + wait() to run: uname, lscpu, lsblk, free, ps, top
 *
 * For each command, the program (parent) forks a child, the child execs the
 * real Linux utility, and the parent waits for it — then prints a short
 * explanation of what that command reveals about OS abstraction.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 16

/* One investigation "step": a command to run + explanation to print after it */
typedef struct {
    const char *label;        // Section title
    char *args[MAX_ARGS];     // argv[] for execvp (NULL-terminated)
    const char *resource;     // Which resource this abstracts
    const char *explanation;  // What to tell the student
} Investigation;

/* Runs one external command via fork() + execvp() + wait(),
   printing PIDs so the process lifecycle stays visible throughout */
void run_command(const char *label, char *args[]) {
    printf("\n----------------------------------------------------------\n");
    printf(">>> %s\n", label);
    printf("    Command: ");
    for (int i = 0; args[i] != NULL; i++) printf("%s ", args[i]);
    printf("\n");
    printf("    [Parent] PID = %d is about to fork...\n", getpid());

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        /* ---- CHILD ---- */
        printf("    [Child]  PID = %d, Parent PID = %d -> exec(%s)\n\n",
               getpid(), getppid(), args[0]);
        execvp(args[0], args);

        /* Only reached if execvp fails (e.g. command not installed) */
        fprintf(stderr, "    execvp failed for '%s': command not found or not permitted\n",
                args[0]);
        exit(EXIT_FAILURE);

    } else {
        /* ---- PARENT ---- */
        int status;
        pid_t finished = wait(&status);

        if (WIFEXITED(status)) {
            printf("\n    [Parent] Child PID %d finished (exit code %d)\n",
                   finished, WEXITSTATUS(status));
        }
    }
}

void print_explanation(const char *resource, const char *explanation) {
    printf("\n    >> OS ABSTRACTION (%s):\n", resource);
    printf("    %s\n", explanation);
}

int main(void) {
    printf("============================================================\n");
    printf(" Hardware <-> OS Abstraction Explorer\n");
    printf(" Investigating shell PID = %d\n", getpid());
    printf("============================================================\n");

    /* --- 1. uname: identify the kernel that provides all abstractions --- */
    char *cmd_uname[] = {"uname", "-a", NULL};
    run_command("uname -a  (Kernel & platform identity)", cmd_uname);
    print_explanation("KERNEL IDENTITY",
        "Every abstraction below (process, virtual memory, VFS, drivers) is a\n"
        "    service compiled into THIS exact kernel. uname tells us which kernel\n"
        "    version/architecture is doing the abstracting.");

    /* --- 2. lscpu: physical/logical CPU topology --- */
    char *cmd_lscpu[] = {"lscpu", NULL};
    run_command("lscpu  (CPU hardware topology)", cmd_lscpu);
    print_explanation("CPU",
        "lscpu shows the REAL core/thread count and cache hierarchy. The kernel's\n"
        "    scheduler uses this physical topology to hand out CPU TIME SLICES to\n"
        "    processes. A process never owns a core directly -- it competes for\n"
        "    scheduled time on one of these logical CPUs.");

    /* --- 3. lsblk: raw block devices vs mounted filesystems --- */
    char *cmd_lsblk[] = {"lsblk", NULL};
    run_command("lsblk  (Raw block devices)", cmd_lsblk);

char *cmd_df[] = {"df", "-h", NULL};
    run_command("df -h  (Mounted filesystems on top of those devices)", cmd_df);
    print_explanation("STORAGE",
        "lsblk shows raw block devices (sectors, no notion of 'files'). df -h shows\n"
        "    the FILESYSTEM layer the kernel's VFS builds on top of those devices.\n"
        "    A program calling open(\"/path/file\") never touches a sector directly --\n"
        "    the VFS translates the path into inode lookups, then block I/O.");

    /* --- 4. free: physical RAM vs virtual memory --- */
    char *cmd_free[] = {"free", "-h", NULL};
    run_command("free -h  (Physical RAM usage)", cmd_free);
    print_explanation("MEMORY",
        "free shows finite PHYSICAL RAM. But each process gets its own private\n"
        "    VIRTUAL address space, translated to physical frames by the kernel's\n"
        "    page tables + MMU. That's why two processes can use the same virtual\n"
        "    addresses without colliding -- the abstraction guarantees isolation.");

    /* --- 5. ps: the process abstraction / PCB --- */
    char *cmd_ps[] = {"ps", "-ef", NULL};
    run_command("ps -ef  (Running processes / PCB view)", cmd_ps);
    print_explanation("PROCESSES",
        "Each row is one PCB: PID, PPID, state, TTY. VSZ vs RSS (see 'ps aux')\n"
        "    differing is direct evidence of virtual memory -- a process can RESERVE\n"
        "    more address space than it currently has backed by physical RAM.");

    /* --- 6. top: live scheduling / resource contention --- */
    char *cmd_top[] = {"top", "-bn1", NULL};
    run_command("top -bn1  (Live snapshot of scheduler activity)", cmd_top);
    print_explanation("SCHEDULING / I-O",
        "top shows how many tasks are runnable/sleeping RIGHT NOW versus how many\n"
        "    physical CPUs exist (from lscpu). When tasks >> CPUs, the kernel is\n"
        "    constantly context-switching, giving each process the illusion of\n"
        "    continuous execution on hardware it's actually sharing.");

    printf("\n============================================================\n");
    printf(" Summary: every command above queried the SAME physical machine,\n");
    printf(" but each layer (uname/lscpu/lsblk/free/ps/top) exposes it through\n");
    printf(" a different OS abstraction that user programs actually rely on.\n");
    printf("============================================================\n");

    return 0;
}

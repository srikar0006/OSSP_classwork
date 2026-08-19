/*
 *
 * Demonstrates file copying using RAW SYSTEM CALLS (not the C stdio
 * library): open(), read(), write(), close().
 *
 * Usage: ./file_copy_syscalls <source_file> <destination_file>
 */

#include <stdio.h>      // for perror(), printf() (these are library calls, not syscalls)
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <unistd.h>     // read(), write(), close()
#include <fcntl.h>      // open(), O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC
#include <sys/stat.h>   // mode constants (file permission bits)
#include <errno.h>      // errno

#define BUFFER_SIZE 4096   // one memory "page" worth of data per read/write

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_file> <destination_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    long total_bytes_copied = 0;

    /* ---------------------------------------------------------
     * STEP 1: open() the source file for reading.
     * This is a SYSTEM CALL -> switches CPU from user mode to
     * kernel mode. The kernel looks up the file via the VFS,
     * checks permissions, allocates a file descriptor, and
     * returns control (and the fd) back to user space.
     * --------------------------------------------------------- */
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("open (source) failed");
        exit(EXIT_FAILURE);
    }
    printf("[User space] Opened source '%s' -> file descriptor %d\n", src_path, src_fd);

    /* ---------------------------------------------------------
     * STEP 2: open() the destination file for writing.
     * O_CREAT  -> create the file if it doesn't exist
     * O_WRONLY -> open write-only
     * O_TRUNC  -> if it exists, truncate to zero length first
     * 0644     -> permission bits (rw-r--r--) used only if created
     * --------------------------------------------------------- */
    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("open (destination) failed");
        close(src_fd);
        exit(EXIT_FAILURE);
    }
    printf("[User space] Opened destination '%s' -> file descriptor %d\n", dst_path, dst_fd);

    /* ---------------------------------------------------------
     * STEP 3: Copy loop.
     * read()  -> trap into kernel, kernel copies data from the
     *            file (via the page cache / disk driver) into
     *            our user-space buffer, returns byte count.
     * write() -> trap into kernel again, kernel copies data from
     *            our user-space buffer into the destination
     *            file's kernel buffers (eventually flushed to disk).
     * --------------------------------------------------------- */
    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {

        bytes_written = write(dst_fd, buffer, bytes_read);

        if (bytes_written < 0) {
            perror("write failed");
            close(src_fd);
            close(dst_fd);
            exit(EXIT_FAILURE);
        }

        if (bytes_written != bytes_read) {
            fprintf(stderr, "Partial write: expected %zd, wrote %zd\n",
                    bytes_read, bytes_written);
            close(src_fd);
            close(dst_fd);
            exit(EXIT_FAILURE);
        }

        total_bytes_copied += bytes_written;
    }

    if (bytes_read < 0) {
        perror("read failed");
        close(src_fd);
        close(dst_fd);
        exit(EXIT_FAILURE);
    }

/* ---------------------------------------------------------
     * STEP 4: close() both file descriptors.
     * Another system call -> kernel releases the fd table entry
     * and flushes any buffered data associated with it.
     * --------------------------------------------------------- */
    if (close(src_fd) < 0) {
        perror("close (source) failed");
        exit(EXIT_FAILURE);
    }
    if (close(dst_fd) < 0) {
        perror("close (destination) failed");
        exit(EXIT_FAILURE);
    }

    printf("[User space] Copy complete: %ld bytes copied from '%s' to '%s'\n",
           total_bytes_copied, src_path, dst_path);

    return 0;
}

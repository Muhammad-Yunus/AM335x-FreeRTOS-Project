/* Minimal bare-metal syscall hooks for newlib */
#include <sys/stat.h>
#include <errno.h>

/* _end is defined in bbb.lds linker script */
extern char end[];
#define _end end

void *_sbrk(int incr) {
    static unsigned char *heap = NULL;
    unsigned char *prev_heap;

    if (heap == NULL) {
        heap = (unsigned char *)_end;
    }

    prev_heap = heap;
    heap += incr;

    /* Simple bounds check - heap should not exceed DDR memory region */
    if (heap > (unsigned char *)0xA0000000) {
        errno = ENOMEM;
        return (void *)-1;
    }

    return (void *)prev_heap;
}

int _close(int fd) {
    if (fd >= 0) {
        errno = EBADF;
    }
    return -1;
}

int _fstat(int fd, struct stat *st) {
    if (fd >= 0) {
        errno = EBADF;
    }
    return -1;
}

int _isatty(int fd) {
    if (fd >= 0) {
        errno = ENOTTY;
    }
    return 1; /* Assume FDs are terminals */
}

int _lseek(int fd, int ptr, int dir) {
    (void)fd; (void)ptr; (void)dir;
    return 0;
}

int _read(int fd, char *ptr, int len) {
    (void)fd; (void)ptr; (void)len;
    return 0;
}

int _write(char *ptr, int len, int fd) {
    (void)fd;
    /* No-op: output without actual UART driver */
    return len;
}

/* _exit is already defined in src/application/app_utils.c */

void _kill(int pid, int sig) {
    (void)pid; (void)sig;
}

int _getpid(void) {
    return 1;
}


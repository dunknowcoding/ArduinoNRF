#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

extern "C" {
extern char _end;
static char *heapEnd = &_end;

void *_sbrk(ptrdiff_t increment) {
    char *previous = heapEnd;
    heapEnd += increment;
    return previous;
}

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *status) {
    (void)file;
    status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _write(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    return len;
}

void _exit(int status) {
    (void)status;
    while (true) {
    }
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}
}

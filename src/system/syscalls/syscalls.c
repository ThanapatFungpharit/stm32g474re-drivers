/**
 * @file syscalls.c
 * @brief Minimal newlib system calls.
 *
 * - _write() sends stdout/stderr to the console (ITM or the output chosen
 *   with console_set_output()), so printf() works. Note that newlib's
 *   printf adds several kilobytes of code.
 * - The other calls report "not supported".
 *
 * All functions are weak; define your own to override them. Without these
 * definitions the linker pulls in libnosys stubs and warns that they "will
 * always fail".
 */

#if !defined(UNIT_TEST_HOST)

#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#include "system/console/console.h"

int _write(int fd, const char *buf, int len);
int _read(int fd, char *buf, int len);
int _close(int fd);
int _lseek(int fd, int offset, int whence);
int _fstat(int fd, struct stat *st);
int _isatty(int fd);

__attribute__((weak)) int _write(const int fd, const char *buf, const int len)
{
    if ((fd != 1) && (fd != 2)) {
        errno = EBADF;
        return -1;
    }
    if (len <= 0) {
        return 0;
    }
    (void)console_write(buf, (size_t)len);
    return len; /* the console is best effort; never stall stdio */
}

__attribute__((weak)) int _read(const int fd, char *buf, const int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    errno = ENOSYS;
    return -1;
}

__attribute__((weak)) int _close(const int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

__attribute__((weak)) int _lseek(const int fd, const int offset, const int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

__attribute__((weak)) int _fstat(const int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR; /* character device: line buffered */
    return 0;
}

__attribute__((weak)) int _isatty(const int fd)
{
    return ((fd >= 0) && (fd <= 2)) ? 1 : 0;
}

#endif /* !UNIT_TEST_HOST */

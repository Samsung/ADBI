#ifndef IO_H_
#define IO_H_

#include "common.h"
#include "syscall_template.h"

/***********************************************************************************************************************
 * File open flags.
 **********************************************************************************************************************/

/* Architecture specific, from libc/kernel/arch-arm/asm/fcntl.h */
#define O_DIRECTORY 040000
#define O_NOFOLLOW 0100000
#define O_DIRECT 0200000
#define O_LARGEFILE 0400000

/* Generic, from libc/kernel/common/asm-generic/fcntl.h */
#define O_ACCMODE 00000003
#define O_RDONLY 00000000
#define O_WRONLY 00000001
#define O_RDWR 00000002
#ifndef O_CREAT
#define O_CREAT 00000100
#endif
#ifndef O_EXCL
#define O_EXCL 00000200
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 00000400
#endif
#ifndef O_TRUNC
#define O_TRUNC 00001000
#endif
#ifndef O_APPEND
#define O_APPEND 00002000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 00004000
#endif
#ifndef O_SYNC
#define O_SYNC 00010000
#endif
#ifndef FASYNC
#define FASYNC 00020000
#endif
#ifndef O_DIRECT
#define O_DIRECT 00040000
#endif
#ifndef O_LARGEFILE
#define O_LARGEFILE 00100000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 00200000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 00400000
#endif
#ifndef O_NOATIME
#define O_NOATIME 01000000
#endif
#ifndef O_NDELAY
#define O_NDELAY O_NONBLOCK
#endif

/* Libc specific (?), from libc/include/fcntl.h */
#ifndef O_CLOEXEC
#define O_CLOEXEC  02000000
#endif

/***********************************************************************************************************************
 * File creation modes.
 **********************************************************************************************************************/

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

/***********************************************************************************************************************
 * Syscall functions.
 **********************************************************************************************************************/

#ifdef __aarch64__

SYSCALL_4_ARGS(get_nr(__NR_openat),
        int, openat, int, const char * path, int flags, int mode);

static int open(const char * path, int flags, int mode) {
    return openat(-100, path, flags, mode);
}

SYSCALL_1_ARGS(get_nr(__NR_close),
        int, close, int fd);

SYSCALL_3_ARGS(get_nr(__NR_read),
        ssize_t, read, int fd, void * buf, int count);

SYSCALL_3_ARGS(get_nr(__NR_write),
        ssize_t, write, int fd, const void * buf, size_t count);

#else

SYSCALL_3_ARGS(get_nr(5),
        int, open, const char * path, int flags, int mode);

SYSCALL_1_ARGS(get_nr(6),
        int, close, int fd);

SYSCALL_3_ARGS(get_nr(3),
        ssize_t, read, int fd, void * buf, int count);

SYSCALL_3_ARGS(get_nr(4),
        ssize_t, write, int fd, const void * buf, size_t count);
#endif

#endif /* IO_H_ */

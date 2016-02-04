#ifndef UNIX_H_
#define UNIX_H_

#include "common.h"
#include "syscall_template.h"

typedef int pid_t;

typedef unsigned int uid_t;
typedef unsigned int gid_t;

SYSCALL_0_ARGS(get_nr(__NR_getpid), pid_t, getpid, void);
SYSCALL_0_ARGS(get_nr(__NR_getuid), pid_t, getuid, void);
SYSCALL_0_ARGS(get_nr(__NR_getgid), pid_t, getgid, void);
SYSCALL_0_ARGS(get_nr(__NR_geteuid), pid_t, geteuid, void);
SYSCALL_0_ARGS(get_nr(__NR_getegid), pid_t, getegid, void);
SYSCALL_0_ARGS(get_nr(__NR_gettid), pid_t, gettid, void);

#endif /* UNIX_H_ */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "tgkill.h"

#ifndef SYS_tgkill
#define SYS_tgkill __NR_tgkill
#endif

int tgkill(int tgid, int tid, int sig) {
    return syscall(SYS_tgkill, tgid, tid, sig);
}

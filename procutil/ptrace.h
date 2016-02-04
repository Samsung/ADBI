#ifndef PTRACE_H
#define PTRACE_H

#include <sys/ptrace.h>
#include <sys/types.h>

#include "process/process.h"

#define is_word_aligned(addr) (((addr) & 0x03) == 0)

bool ptrace_signal(pid_t pid, pid_t tid, int signo);

bool ptrace_attach(pid_t pid, pid_t tid);
bool ptrace_detach(pid_t pid);

int ptrace_stop(pid_t pid);

int ptrace_wait_single(pid_t pid, int * signo);

bool ptrace_mem_write(pid_t pid, address_t address, unsigned long data);
bool ptrace_mem_read(pid_t pid, address_t address, unsigned long * __restrict data);

void ptrace_set_options(pid_t pid);
pid_t ptrace_get_child_pid(pid_t pid);

#endif

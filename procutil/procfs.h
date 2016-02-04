#ifndef PROCFS_H
#define PROCFS_H

#include <sys/types.h>

typedef struct segment_t segment_t;
typedef struct thread_t thread_t;

pid_t procfs_get_tgid(pid_t pid);
pid_t procfs_get_ppid(pid_t pid);
pid_t procfs_get_tracerpid(pid_t pid);

bool procfs_iter_threads(pid_t pid, void (fn)(pid_t tid));

char procfs_pid_state(pid_t pid, pid_t tid);

bool procfs_iter_segments(const thread_t * thread, void (fn)(const segment_t * segment));

size_t procfs_mem_read(thread_t * thread, address_t offset, size_t size, void * out);

bool procfs_address_executable(const thread_t * thread, address_t address);

const char * procfs_get_exe(pid_t pid, pid_t tid);

#endif

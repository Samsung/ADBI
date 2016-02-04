#ifndef THREAD_H
#define THREAD_H

#include <sys/types.h>
#include "tree.h"
#include "procutil/ptrace.h"

struct process_t;
typedef struct process_t process_t;

typedef struct thread_t {
    struct process_t * process;
    pid_t pid;
    
    struct {
        bool running;       /* is the thread running? */
        bool slavemode;     /* is the thread is doing slave work for adbi server? */
        bool execed;        /* is was the last stop caused by an exec? */
        bool stopme;        /* are we planning to stop the thread? */
        int signo;          /* signal to be deliver on next continue */
        bool setoptions;    /* do we need to set ptrace options? */
        bool dead;          /* is the thread dead? */
    } state;

    bool notified;      /* was new thread handlers triggered? */
    
    refcnt_t references;
    
} thread_t;

bool thread_is_mode32(thread_t * thread);

thread_t * thread_create(process_t * process, pid_t pid);
void thread_free(thread_t * thread);

void thread_continue(thread_t * thread, int signo);
void thread_continue_or_stop(thread_t * thread, int signo);
void thread_stop(thread_t * thread);

thread_t * thread_attach(process_t * process, pid_t pid);
void thread_detach(thread_t * thread);
void thread_kill(thread_t * thread);

void thread_exec(thread_t * thread);
void thread_exit(thread_t * thread);
bool thread_trap(thread_t * thread);

bool thread_get_regs(thread_t * thread, pt_regs * regs);
bool thread_set_regs(thread_t * thread, pt_regs * regs);

void thread_wait(thread_t * thread, bool nohang);

bool thread_check_alive(thread_t * thread);

bool thread_is_stable(thread_t * thread);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <linux/elf.h>
#include <linux/uio.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "thread.h"
#include "process.h"
#include "list.h"
#include "spawn.h"

#include "procutil/tgkill.h"

#include "tree.h"
#include "segment.h"
#include "linker.h"
#include "procutil/procfs.h"
#include "injection/injection.h"

#define ADBI_PTRACE_OPTINS (PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC)

static void thread_init_options(thread_t * thread) {
    if (unlikely(ptrace(PTRACE_SETOPTIONS, thread->pid, NULL, (void *) ADBI_PTRACE_OPTINS) == -1))
        adbi_bug("Error setting ptrace options for %d: %s", thread->pid, strerror(errno));
}

bool thread_check_alive(thread_t * thread) {
    if (thread->state.dead)
        return false;
        
    thread_wait(thread, true);
    return !thread->state.dead;
}

static void thread_handle_ptrace_error(thread_t * thread, const char * action, int error) {
    adbi_bug("Error %s thread %d:%d: %s.", action, thread->process->pid, thread->pid, strerror(error));
    
    assert(error == ESRCH);
    
    /* No such process, the process must be dead, but we didn't wait on it yet. The process should exist as a
     * zombie. We can now wait on it.  */
    
    thread_check_alive(thread);
    assert(thread->state.dead);
}

bool thread_is_mode32(thread_t * thread) {
    pt_regs regs;
    thread_get_regs(thread, &regs);
    return arch_is_mode32(&regs);
}

thread_t * thread_create(process_t * process, pid_t pid) {
    thread_t * thread = adbi_malloc(sizeof(struct thread_t));
    
    thread->pid = pid;
    thread->process = process;
    
    /* set default state -- it may need to be adjusted */
    thread->state.setoptions = true;
    thread->state.running = false;
    thread->state.execed = false;
    thread->state.stopme = false;
    thread->state.slavemode = false;
    thread->state.dead = false;
    
    thread->notified = true;

    thread->state.signo = 0;
    
    /* there are two references: one on the thread list and one on the process thread list */
    thread->references = 1;
    
    process_dup(process);   /* the thread holds a pointer to the process, protect it */
    thread_add(thread);
    
    debug("Created thread %s.", str_thread(thread));
    return thread;
}

void thread_free(thread_t * thread) {
    process_t * process = thread->process;
    assert(thread);
    assert(thread->references == 0);
    
    debug("Freed thread %s.", str_thread(thread));
    free(thread);
    
    if (tree_empty(&process->threads))
        process_del(process);
        
    process_put(process);   /* the thread doesn't need the process pointer anymore */
}

thread_t * thread_attach(process_t * process, pid_t tid) {
    if (!ptrace_attach(process->pid, tid))
        return NULL;
        
    /* attached */
    if (procfs_get_tgid(tid) != process->pid) {
        /* The thread we attached to does not belong to process. This can mean that the thread died before we
         * attached and another thread with the same pid was created in its place. */
        if (unlikely(ptrace(PTRACE_DETACH, tid, 0, NULL) == -1)) {
            adbi_bug("Error detaching from thread, which was attached by mistake.")
        }
        return NULL;
    }
    return thread_create(process, tid);
}

/* Detach the given thread. The thread must be stopped. The pointer becomes invalid after this call. */
void thread_detach(thread_t * thread) {
    assert(!thread->state.running);
    
    if (likely(ptrace(PTRACE_DETACH, thread->pid, 0, NULL) != -1)) {
        verbose("Detached %s.", str_thread(thread));
        thread_del(thread);
    } else {
        thread_handle_ptrace_error(thread, "detaching", errno);
    }
}

static bool thread_signal(thread_t * thread, int signo) {
    debug("Sending signal %s to %s.", str_signal(signo), str_thread(thread));
    if (tgkill(thread->process->pid, thread->pid, signo) != 0) {
        thread_handle_ptrace_error(thread, "sending signal to", errno);
        return false;
    } else
        return true;
}

void thread_kill(thread_t * thread) {
    thread_signal(thread, SIGKILL);
    thread->state.stopme = false;
    while (!thread->state.dead)
        thread_wait(thread, false);
}

void thread_exit(thread_t * thread) {
    if (spawn_is_spawned(thread->pid))
        spawn_died(thread->pid);
    thread->state.running = false;
    thread->state.dead = true;
    thread_del(thread);
}

/* Set ptrace options of the given process and enable tracing of clone, fork and exec calls. */
void thread_exec(thread_t * thread) {
    process_t * process = thread->process;
    
    info("Thread %s execing.", str_thread(thread));
    
    thread_init_options(thread);
    
    /* Clear all threads except for the calling thread */
    {
        void callback(thread_t * other) {
            assert(other->process == thread->process);
            if (other != thread) {
                verbose("Thread %s died during exec.", str_thread(other));
                thread_exit(other);
            }
        }
        thread_iter(process, callback);
    }
    
    thread->state.execed = true;
    
    /* After exec all old memory segments are gone... */
    segment_reset(process);
    
    /* ...so are injections... */
    injection_reset(process);
    
    thread->process->mode32 = thread_is_mode32(thread);

    /* ...but new segments usually appear immediately... */
    inject_adbi(thread);
    segment_rescan(thread);
    
    /* ...so the linker breakpoint may need to be reinserted. */
    linker_reset(thread);
    
    thread_continue_or_stop(thread, 0);
}



void thread_continue(thread_t * thread, int signo) {

    if (unlikely(thread->state.dead || thread->state.running))
        return;
        
    if (unlikely(thread->state.setoptions)) {
        thread_init_options(thread);
        thread->state.setoptions = false;
    }

    if (likely(!signo)) {
        signo = thread->state.signo;
        thread->state.signo = 0;
    }

    if (unlikely(signo)) {
        debug("Delivering signal %s to %s.", str_signal(signo), str_thread(thread));
    } else {
        //debug("Resuming %s.", str_thread(thread));
    }
    
    if (likely(ptrace(PTRACE_CONT, thread->pid, 0, (void *)(intptr_t) signo)) != -1) {
        thread->state.running = true;
    } else {
        thread_handle_ptrace_error(thread, "continuing", errno);
    }
    
}

void thread_continue_or_stop(thread_t * thread, int signo) {

    if (unlikely(thread->state.dead || thread->state.running))
        return;
        
    if (unlikely(thread->state.stopme || thread->state.slavemode)) {
        /* the thread was requested to stop */
        if (thread->state.slavemode) {
            thread->state.signo = signo;
        } else {
            thread->state.signo = signo == SIGSTOP ? 0 : signo;
        }
        if (thread->state.stopme)
            thread->state.stopme = false;
    } else {
        if (unlikely(!thread->notified))
            injection_notify_new_thread(thread);

        /* the thread can be continued immediately */
        thread_continue(thread, signo);
    }
    
}

/* Stop the given thread.  This has no effect if the thread is already stopped or dead.  Moreover, the thread might die
 * during stopping (e.g. when it crashes before SIGSTOP is delivered). */
void thread_stop(thread_t * thread) {
    if (thread->state.dead || !thread->state.running)
        return;
        
    thread->state.stopme = true;
    thread_signal(thread, SIGSTOP);
    
    thread_wait(thread, false);
    assert(!thread->state.running);
    
}


bool thread_get_regs(thread_t * thread, pt_regs * regs) {
    assert(!thread->state.running);
#ifndef PTRACE_GETREGS
    struct iovec iov = { regs, sizeof(*regs) };
    if (unlikely(ptrace(PTRACE_GETREGSET, thread->pid, NT_PRSTATUS, &iov))) {
#else
    if (unlikely(ptrace(PTRACE_GETREGS, thread->pid, NULL, regs))) {
#endif
        thread_handle_ptrace_error(thread, "reading registers of", errno);
        return false;
    }
    return true;
}

bool thread_set_regs(thread_t * thread, pt_regs * regs) {
    assert(!thread->state.running);
#ifndef PTRACE_SETREGS
    struct iovec iov = { regs, sizeof(*regs) };
    if (unlikely(ptrace(PTRACE_SETREGSET, thread->pid, NT_PRSTATUS, &iov))) {
#else
    if (unlikely(ptrace(PTRACE_SETREGS, thread->pid, NULL, regs))) {
#endif
        thread_handle_ptrace_error(thread, "setting registers of", errno);
        return false;
    }
    return true;
}

bool thread_is_stable(thread_t * thread) {
    pt_regs regs;
    assert(!thread->state.running);
    
    if (thread->state.dead)
        return true;
        
    if (!thread_get_regs(thread, &regs)) {
        /* The thread died while reading registers. */
        return true;
    }
    
    address_t insn_ptr = arch_get_pc(&regs);
    process_t * process = thread->process;
    
    return !address_is_artificial(process, insn_ptr);
}

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <unistd.h>

#include "tree.h"

#include "injection/inject.h"

#include "util/signal.h"

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"
#include "process/list.h"

#include "process/spawn.h"
#include "process/linker.h"

#include "procfs.h"
#include "ptrace.h"
#include "wait.h"

static thread_t * thread_clone_create_child(pid_t child_pid) {
    thread_t * child_thread;
    pid_t child_tgid = procfs_get_tgid(child_pid);
    process_t * parent_process = process_get(child_tgid);
    process_t * child_process;
    
    if (!parent_process) {
        /* A new process was created. */
        pid_t child_ppid = procfs_get_ppid(child_pid);
        parent_process = process_get(child_ppid);
        info("New process %d (spawned by %s).", child_pid, str_process(parent_process));
        child_process = process_forked(parent_process, child_pid);
    } else {
        /* A new thread in an existing process was created. */
        child_process = process_dup(parent_process);
        info("New thread %d:%d.", child_process->pid, child_pid);
    }
    
    child_thread = thread_create(child_process, child_pid);
    child_thread->notified = false;

    process_put(parent_process);
    process_put(child_process);
    
    return child_thread;
}

static void thread_handle_clone_child(pid_t child_pid) {
    /* child is stopped */
    assert(procfs_get_tracerpid(child_pid) == getpid());
    
    thread_t * child_thread = thread_clone_create_child(child_pid);
    
    thread_continue_or_stop(child_thread, 0);
    
    thread_put(child_thread);
}

static void thread_handle_clone_parent(thread_t * parent_thread, pid_t child_pid) {
    /* parent is stopped, child is detached may be stopped, running or zombie */
    thread_t * child_thread = thread_get(child_pid);
    
    if (child_thread) {
        /* We've received a notification from the child process already. We don't need to do anything here. */
        debug("Process %s confirmed creation of %s.", str_thread(parent_thread), str_thread(child_thread));
    } else {
        child_thread = thread_clone_create_child(child_pid);
        
        /* From our perspective the child is running (we didn't receive a SIGSTOP) and detached. */
        child_thread->state.running = true;
    }
    
    thread_continue_or_stop(parent_thread, 0);
    thread_put(child_thread);
}

static void thread_handle_status(thread_t * thread, int status) {

    assert(thread->references >= 1);    /* list + current lock */
    assert((thread->references >= 2) || (thread->state.dead));  /* list + current lock */
    
    thread->state.running = false;
    
    if (likely(WIFSTOPPED(status))) {
        /* The process was stopped by a signal. */
        int signo = WSTOPSIG(status);
        
        if ((signo == SIGTRAP) && ((status >> 16) & 0xf)) {
            /* We catched a fork, vfork, clone or execve. */
            
            thread->state.setoptions = true;
            
            if ((status >> 16) == PTRACE_EVENT_EXEC) {
                /* exec */
                thread_exec(thread);
            } else {
                /* clone, vfork, fork */
                pid_t child_pid = ptrace_get_child_pid(thread->pid);
                thread_handle_clone_parent(thread, child_pid);
            }
            
            return;
        }
        
        if (likely(!thread->state.slavemode && ((signo == SIGILL) || (signo == SIGTRAP) || (signo == SIGBUS)))) {
            if (thread_trap(thread))
                return;
        }

        if (thread->process->stabilizing && (signo == SIGSEGV)) {
            /* Process is stabilizing. */
            pt_regs regs;
            
            if (!thread_get_regs(thread, &regs)) {
                /* Thread died while reading registers. Consider this case as handled. */
                return;
            }
            
            if (segment_check_address_is_code(thread->process, instruction_pointer(&regs))) {
                /* The PC points to code, leave the thread stopped. */
                info("Stabilization stop of thread %s at %s.", str_thread(thread),
                     str_address(thread->process, instruction_pointer(&regs)));
                thread->state.signo = signo;
                return;
            } else {
                /* PC doesn't point to code, this must be a regular SIGSEGV.  Continue normal processing. */
                warning("Thread %s received unexpected SIGSEGV while stabilizing.", str_thread(thread));
            }
        }
        
        if (!thread->state.slavemode) {
            switch (signo) {
                case SIGSEGV:
                case SIGBUS:
                case SIGILL:
                case SIGTRAP:
                    dump_thread(thread);
                default:
                    ;
            }
        }
        thread_continue_or_stop(thread, signo);
        
    } else if (unlikely(WIFEXITED(status)) || WIFSIGNALED(status)) {
        /* The process exited or was terminated by a signal. We don't really care, because all we have to do is delete
         * this thread. */
        
        if (WIFEXITED(status)) {
            verbose("Thread %s exited normally returning %d.",
                    str_thread(thread), WEXITSTATUS(status));
        } else {
            info("Thread %s was terminated by signal %s %s.",
                 str_thread(thread),
                 str_signal(WTERMSIG(status)),
                 WCOREDUMP(status) ? " producing a core dump" : "");
        }
        
        thread_exit(thread);
    } else {
        adbi_bug_unrechable();
    }
}

static void thread_handle_status_unknown(pid_t pid, int status) {

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
    
        if (WIFEXITED(status)) {
            verbose("Untraced process %d exited normally returning %d.", pid,
                    WEXITSTATUS(status));
        } else {
            info("Untraced process %d was terminated by signal %s%s.", pid,
                 str_signal(WTERMSIG(status)),
                 WCOREDUMP(status) ? " producing a core dump" : "");
        }
        
        if (spawn_is_spawned(pid))
            spawn_died(pid);
        else
            error("Unexpected death notification from process %d.", pid);
            
        return;
    }
    
    /* We received an event from an untraced process. This means that Linux automatically attached us to the process,
     * because it was spawned by one of our traced processes. We should receive an event from the parent shortly, but
     * we'll find the thread's creator anyway. */
    if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGSTOP)) {
        thread_handle_clone_child(pid);
    }
    
}

void thread_wait(thread_t * thread, bool nohang) {
    if (unlikely(thread->state.dead))
        return;
    int status;
    int res = waitpid(thread->pid, &status, WUNTRACED | __WALL | (nohang ? WNOHANG : 0));
    
    if (unlikely(res == 0)) {
        /* No child has changed state. */
        assert(nohang);
        return;
    }
    
    if (unlikely(res == -1)) {
        debug("Wait on %s failed.", str_thread(thread));
        assert(errno == ECHILD);
        thread_exit(thread);
        return;
    }
    
    assert(res == thread->pid);
    //debug("Waited on %s.", str_thread(thread));
    thread_handle_status(thread, status);
}

void wait_main() {

    int count = 128;
    
    while (count--) {
        int status;
        int res = waitpid(-1, &status, WNOHANG | WUNTRACED | __WALL);
        
        if (res > 0) {
            thread_t * thread = thread_get(res);
            
            if (likely(thread)) {
                debug("Waited returned %s.", str_thread(thread));
                thread_handle_status(thread, status);
                thread_put(thread);
            } else {
                debug("Waited returned unknown PID %d.", res);
                thread_handle_status_unknown(res, status);
            }
        } else {
            /* handled all pending child signals */
            signal_child = 0;
            break;
        }
    }
    
}

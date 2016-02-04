#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "procfs.h"
#include "ptrace.h"
#include "signal.h"

#include "tgkill.h"

/* Send a signal to a process. Send the signal without using ptrace. Return
 * non-zero on success. */
bool ptrace_signal(pid_t pid, pid_t tid, int signo) {

    int res = (tgkill(pid, tid, signo) == 0);
    
    if (res) {
        info("Sent signal %d (%s) to %d:%d.", signo, strsignal(signo), pid, tid);
    } else {
        error("Error sending %d (%s) to %d:%d: %s.", signo, strsignal(signo), pid, tid, strerror(errno));
    }
    
    return res;
}

/* Perform a stop-start operation on a newly attached process.
 *
 * Attached processes should receive a SIGSTOP signal shortly after an
 * attaching request. If the process was in (normal) stopped state, it may
 * still remain in that state. What we need, however, is to have the process
 * stopped in a special tracing stop state. To assure that, we need to send
 * an SIGSTOP explicitly and let the program continue. As there can only be one
 * SIGSTOP signal queued, we will not have to deal with duplicated stops when
 * calling waitpid. */
static void ptrace_attach_stopstart(pid_t pid, pid_t tid) {

    if (procfs_pid_state(pid, tid) == 'T') {
        info("Process %d needs stop-starting.", tid);
        
        /* None of the following calls should fail at this point. */
        if (!ptrace_signal(pid, tid, SIGSTOP))
            adbi_bug("Error sending SIGSTOP to process %d.");
            
        if (ptrace(PTRACE_CONT, tid, NULL, NULL))
            adbi_bug("Error continuing process %d: %s", tid, strerror(errno))
            
            verbose("Successfully stop-started process %d.", tid);
    } else {
        verbose("Process %d does not need stop-starting.", tid);
    }
    
}

/* Attach to a process (a single LWP). Block until completion. Attached threads are initially stopped. Return true if
 * if the process is attached, false if not (because the thread exited or did not exist). */
bool ptrace_attach(pid_t tgid, pid_t pid) {

    info("Attaching to process %d...", pid);
    
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) != 0) {
        if (errno == ESRCH)
            /* The process does not exist (anymore). */
            return 0;
            
        /* Anything else should never happen. */
        adbi_bug("Error attaching to process %d: %s.", pid, strerror(errno));
    }
    
    /* Do a stop-start if necessary. */
    ptrace_attach_stopstart(tgid, pid);
    
    /* Wait for the SIGSTOP. */
    while (1) {
        int status;
        pid_t res;
        
        res = waitpid(pid, &status, WUNTRACED | __WALL);
        
        if (res == pid) {
        
            if (WIFSTOPPED(status)) {
                int signo = WSTOPSIG(status);
                
                if (signo == SIGSTOP) {
                    /* Process stopped by stop signal. */
                    return 1;
                } else {
                    /* Process stopped by some other signal. */
                    warning("Process %d was stopped by unexpected signal %s while attaching.",
                            pid, strsignal(WSTOPSIG(status)));
                            
                    /* Deliver the signal. */
                    ptrace(PTRACE_CONT, pid, NULL, &signo);
                }
            } else if (WIFSIGNALED(status)) {
                /* Process was terminated by a signal. */
                warning("Process %d was terminated by signal %s while attaching.", pid, strsignal(WTERMSIG(status)));
                return 0;
            } else if (WIFEXITED(status)) {
                /* Process exited normally. */
                warning("Process %d exited with status %d while attaching.", pid, WEXITSTATUS(status));
                return 0;
            }
            
            #ifdef WIFCONTINUED
            else if (WIFCONTINUED(status)) {
                /* Continued, we probably shouldn't receive this notification. */
                warning("Process %d was continued.", pid);
            }
            #endif
            
        } else {
            if (res < 0) {
                adbi_bug("The waitpid function failed: %s.", strerror(errno));
            } else {
                adbi_bug("The waitpid function returned %d, but %d was expected.", res, pid);
            }
        }
        
    }
    
    adbi_bug_unrechable();
}







#if 0

/* Wait for an event from a single process.
 * Return:
 *      1   if the process was stopped by a signal
 *      0   if waiting failed
 *      -1  if the process exited normally
 *      -2  if the process was terminated by a signal
 *
 * If the result is 1 or -2, the *signo is set to signal number. If signo is
 * NULL, it is ignored. */
int ptrace_wait_single(pid_t pid, int * __restrict signo) {

    int status;
    int res;
    
    info("Waiting for event from process %d.", (int) pid);
    res = waitpid(pid, &status, WUNTRACED | __WALL);
    
    if (res != pid) {
        if (res < 0) {
            adbi_bug("The waitpid function failed: ", strerror(errno));
        } else {
            adbi_bug("The waitpid function returned %d, but %d was expected.", res, pid);
        }
    }
    
    /* The process stopped. */
    if (WIFSTOPPED(status)) {
    
        if (signo)
            *signo = WSTOPSIG(status);
            
        info("Process %d was stopped by signal %d.", pid, WSTOPSIG(status));
        
        return 1;
        
    } else if (WIFSIGNALED(status)) {
    
        info("Process %d was terminated by signal %d.", pid, WTERMSIG(status));
        return -2;
        
    } else if (WIFEXITED(status)) {
    
        info("Process %d exited with status %d.", pid, WEXITSTATUS(status));
        return -1;
        
    }
    
    adbi_bug_unrechable();
}

#endif

bool ptrace_mem_write(pid_t pid, address_t address, unsigned long data) {

    assert(is_word_aligned(address));
    
    if (ptrace(PTRACE_POKETEXT, pid, (void *) address, (void *) data)) {
        error("Error writing memory at %p in process %d: %s", (void *) address, pid, strerror(errno));
        return false;
    } else
        return true;
        
}

bool ptrace_mem_read(pid_t pid, address_t address, unsigned long * __restrict data) {

    union {
        long ptrace_ret;
        unsigned long value;
    } result;
    
    assert(is_word_aligned(address));
    
    errno = 0;
    result.ptrace_ret = ptrace(PTRACE_PEEKTEXT, pid, (void *) address, NULL);
    
    if (errno) {
        error("Error reading memory at %p in process %d: %s", (void *) address, pid, strerror(errno));
        return false;
    } else {
        /* Returned value is a signed long, we need an unsigned int. */
        *data = result.value;
        return true;
    }
    
}

pid_t ptrace_get_child_pid(pid_t pid) {
    pid_t result;
    if (ptrace(PTRACE_GETEVENTMSG, pid, NULL, &result) == -1)
        adbi_bug("Error reading child PID of process %d: %s", pid, strerror(errno));
    return result;
}

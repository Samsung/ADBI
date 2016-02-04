#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "util/signal.h"
#include "procutil/ptrace.h"
#include "tree.h"

static tree_t spawned_pids = NULL;

bool spawn_is_spawned(pid_t pid) {
    return tree_contains(&spawned_pids, pid);
}

void spawn_died(pid_t pid) {
    tree_remove(&spawned_pids, pid);
}

static bool spawn_attach(pid_t tgid, pid_t pid) {
    int status;
    int res;
    
    debug("Waiting for spawned process %d.", (int) pid);
    res = waitpid(pid, &status, WUNTRACED);
    
    if (res != pid) {
        if (res < 0) {
            adbi_bug("The waitpid function failed: %s.", strerror(errno));
        } else {
            adbi_bug("The waitpid function returned %d, but %d was expected.", res, pid);
        }
    }
    
    if (WIFSTOPPED(status)) {
        /* Process received a signal, but is alive and we can control it. */
        int signo = WSTOPSIG(status);
        
        if (signo != SIGTRAP) {
            /* This isn't the signal we're waiting for. */
            error("Process %d received an unexpected signal %s while attaching after spawn.",
                  (int) pid, str_signal(signo));
            ptrace_signal(tgid, pid, SIGKILL);
            ptrace(PTRACE_DETACH, pid, NULL, NULL);
            return false;
        }
        
        info("Spawned %d.", (int) pid);
        return true;
    }
    
    if (WIFEXITED(status)) {
        /* Process exited normally. This means that the execve call failed. */
        error("Spawning failed: %s.", strerror(WEXITSTATUS(status)));
        return false;
    }
    
    /* Anything below this is actually a fatal error. */
    if (WIFSIGNALED(status)) {
        /* Process was (already) terminated by signal. Getting here means that there is a bug in adbiserver. */
        error("Process %i killed by signal %s.", (int) pid, str_signal(WTERMSIG(status)));
        return false;
    }
    
    adbi_bug("Invalid event received from %d.", (int) pid);
    return false;
}

pid_t spawn_process(const char * const * argv) {

    pid_t pid = fork();
    
    if (pid < 0) {
        error("Error forking: %s.", strerror(errno));
        return 0;
    }
    
    if (pid == 0) {
        /* Remove signal masks. */
        signal_reset();
        
        /* This is the child process. */
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        
        setpgid(0, 0);
        
        execv(argv[0], (char * const *) argv);
        if (errno == ENOENT)
            execvp(argv[0], (char * const *) argv);
            
        /* Reaching this point means that execve failed. */
        _exit(errno);
    }
    
    /* Attach to the forked process. */
    if (spawn_attach(pid, pid)) {
        /* Attached. Remember to wait on it later. */
        tree_insert(&spawned_pids, pid, (void *)(intptr_t) pid);
        return pid;
    } else
        return 0;
}

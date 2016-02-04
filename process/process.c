#include <stdlib.h>
#include <sched.h>      /* for sched_yield */
#include <signal.h>

#include "process.h"
#include "thread.h"
#include "list.h"

#include "segment.h"
#include "linker.h"
#include "spawn.h"

#include "configuration/state.h"

#include "procutil/procfs.h"

#include "injection/inject.h"

/**********************************************************************************************************************/

/* Create a new process with the given PID. The process is refcounted. */
static process_t * process_create(pid_t pid) {
    process_t * process = adbi_malloc(sizeof(struct process_t));
    
    process->pid = pid;
    
    process->threads = NULL;
    process->segments = NULL;
    process->injections = NULL;
    process->jumps = NULL;
    process->references = 1;
    process->stabilizing = false;
    
    process->linker.bkpt = 0;
    
    process_add(process);
    
    return process;
}

void process_free(process_t * process) {
    assert(process);
    assert(process->references == 0);
    assert(tree_empty(&process->threads));
    
    segment_reset(process);
    injection_reset(process);
    
    debug("Freed process %s.", str_process(process));
    
    free(process);
}

/**********************************************************************************************************************/

void process_continue(process_t * process) {
    void callback(thread_t * thread) {
        thread_continue(thread, 0);
    }
    thread_iter(process, callback);
}

void process_stop(process_t * process) {
    while (!process_is_stopped(process)) {
        thread_iter(process, thread_stop);
    }
}

/**********************************************************************************************************************/

void process_continue_all() {
    process_iter(process_continue);
}

bool process_any_running() {
    bool ret = false;
    void callback(process_t * process) {
        ret = ret || !process_is_stopped(process);
    }
    process_iter(callback);
    return ret;
}

static void process_stop_stabilize(process_t * process) {
    process_stop(process);
    process_stabilize(process);
}

/* Stop and stabilize all threads. */
void process_stop_all() {
    while (process_any_running())
        process_iter(process_stop_stabilize);
}

/**********************************************************************************************************************/

bool process_is_running(process_t * process) {
    bool running = false;
    void callback(thread_t * thread) {
        running = running && thread->state.running;
    }
    thread_iter(process, callback);
    return running;
}

bool process_is_stopped(process_t * process) {
    bool running = false;
    void callback(thread_t * thread) {
        running = running || thread->state.running;
    }
    thread_iter(process, callback);
    return !running;
}

/**********************************************************************************************************************/

/* Detach from the given processes. */
void process_detach(process_t * process) {

    process_stop_stabilize(process);
    
    thread_t * thread = thread_any_stopped(process);
    
    if (!thread) {
        /* The process met its destiny during detaching. */
        return;
    }
    
    segment_detach(thread);
    injection_detach(thread);
    linker_detach(thread);
    
    thread_put(thread);
    
    /* Detach from threads. Do this *after* all final cleanups in the inferior memory space. */
    thread_iter(process, thread_detach);

    kill(process->pid, SIGCONT);
}

void process_kill(process_t * process) {
    assert(process);
    thread_iter(process, thread_kill);
}

/* Detach from all processes. */
void process_detach_all() {
    process_iter(process_detach);
}

static void process_cleanup_single(process_t * process) {
    if (process->spawned) {
        process_kill(process);
    } else {
        process_detach(process);
    }
}

void process_cleanup() {
    process_iter(process_cleanup_single);
}

/* Spawn a new process. The returned process refcounted. */
process_t * process_spawn(const char ** argv) {

    pid_t pid;
    process_t * process = NULL;
    thread_t * thread;
    
    if ((pid = spawn_process(argv)) <= 0)
        return NULL;
        
    process = process_create(pid);
    process->spawned = true;
    thread = thread_create(process, pid);
    
    thread->state.execed = true;
    
    thread->process->mode32 = thread_is_mode32(thread);

    inject_adbi(thread);
    segment_rescan(thread);
    linker_attach(thread);
    
    thread_put(thread);
    
    if (state_tracing()) {
        /* trace is running -- resume the process */
        process_continue(process);
    }
    
    return process_put(process);
}

/**********************************************************************************************************************/

process_t * process_attach(pid_t pid) {

    process_t * process = NULL;
    thread_t * thread = NULL;
    pid_t tracer;
    int changes = 1;
    
    if (procfs_get_tgid(pid) != pid) {
        error("Error attaching to PID %d -- it is a thread, not a process.", pid);
        return NULL;
    }
    
    if ((tracer = procfs_get_tracerpid(pid)) != 0) {
        error("Error attaching to PID %d -- it is already traced by process %d.", pid, tracer);
        return NULL;
    }
    
    process = process_create(pid);
    process->spawned = false;
    
    do {
        void callback(pid_t thread_pid) {
            thread_t * thread = thread_get(thread_pid);
            
            if (thread) {
                /* We already attached to this thread in a previous loop. */
                assert(thread->process == process);
                thread_put(thread);
                return;
            }
            
            if ((thread = thread_attach(process, thread_pid))) {
                /* Attached. */
                thread_put(thread);
            } else {
                /* Not attached, but this is not an error (probably the thread just exited before our request was
                 * sent). */
            }
            ++changes;
        }
        changes = 0;
        if (!procfs_iter_threads(pid, callback))
            goto fail;
    } while (changes);
    
    thread = thread_any_stopped(process);
    
    if (!thread) {
        /* All newly attached threads should be alive and stopped, if none was found, we didn't attach to any thread. */
        assert(tree_empty(&process->threads));
        goto fail;
    }
    thread->process->mode32 = thread_is_mode32(thread);
    
    inject_adbi(thread);
    segment_rescan(thread);
    linker_attach(thread);
    
    thread_put(thread);
    
    if (state_tracing()) {
        /* trace is running -- resume the process */
        process_continue(process);
    }
    
    return process_put(process);
    
fail:
    /* Detach from any threads. */
    process_detach(process);
    process_del(process);
    return process_put(process);
}

/* Notify about process fork. Return the child process. */
process_t * process_forked(process_t * parent, pid_t child_pid) {
    process_t * child = process_create(child_pid);
    
    child->spawned = parent->spawned;
    
    /* First clone injection information, because segment information may reference injections. */
    injection_fork(child, parent);
    
    segment_fork(child, parent);
    
    linker_fork(child, parent);
    
    return child;
}

/**********************************************************************************************************************/

void process_detach_injectable(process_t * process, const injectable_t * injectable) {
    thread_t * thread = thread_any_stopped(process);
    if (thread) {
        injection_t * injection = segment_detach_injectable(thread, injectable);
        /* the injectable should now be unused in the process, it should be safe to remove it. */
        if (injection) {
            injection_detach_single(thread, injection);
        } else {
            /* there was no injection */
        }
        thread_put(thread);
    } else {
        /* the process died -- nothing to do */
    }
}

void process_attach_injectable(process_t * process, const injectable_t * injectable) {
    thread_t * thread = thread_any_stopped(process);
    if (thread) {
        segment_attach_injectable(thread, injectable);
        thread_put(thread);
    } else {
        /* the process died -- nothing to do */
    }
}

/**********************************************************************************************************************/

static void process_find_unstable(process_t * process, tree_t * unstable) {
    tree_clear(unstable);
    void callback(thread_t * thread) {
        if (unlikely(!thread_is_stable(thread))) {
            thread_dup(thread);
            tree_insert(unstable, (tree_key_t) thread, thread);
        }
    }
    thread_iter(process, callback);
}

/* Stabilize a stopped process. */
void process_stabilize(process_t * process) {
    assert(process_is_stopped(process));
    assert(!process->stabilizing);
    
    tree_t unstable = NULL;
    
    process_find_unstable(process, &unstable);
    
    if (tree_empty(&unstable)) {
        debug("Stopped process %s is stable.", str_process(process));
        return;
    }
    
    verbose("Stopped process %s is unstable.  Stabilizing %d thread%s.", str_process(process),
            tree_size(&unstable),
            tree_size(&unstable) == 1 ? "" : "s");
            
    /* Make all natural segments execution-protected. */
    segment_set_exacutable_all(process, false);
    
    process->stabilizing = true;
    
    /* Continue the unstable threads. */
    TREE_ITER(&unstable, node) {
        thread_t * thread = node->val;
        thread_continue(thread, 0);
    }
    
    int still_running;
    do {
        /* Relinquish the CPU to raise the chance of a context switch to the traced processes. */
        sched_yield();
        
        still_running = 0;
        TREE_ITER(&unstable, node) {
            thread_t * thread = node->val;
            
            if (thread->state.running) {
                /* Thread is running, try to wait upon it. */
                thread_wait(thread, true);
                if (thread->state.signo == SIGSEGV)
                    thread->state.signo = 0;
            }
            
            if (thread->state.running) {
                /* Still running? */
                ++still_running;
            }
        }
    } while (still_running > 0);
    
    /* Release the threads and the tree. */
    thread_t * thread;
    while ((thread = tree_pop(&unstable)))
        thread_put(thread);
        
    process->stabilizing = false;
    verbose("Stabilization of process %s completed.", str_process(process));
    
    /* All threads are now stable.  Revert protection flags. */
    segment_set_exacutable_all(process, true);
    
    process_find_unstable(process, &unstable);
    assert(tree_empty(&unstable));
    
}


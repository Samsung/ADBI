#include "process.h"
#include "thread.h"
#include "list.h"

#include "tree.h"

static tree_t processes = NULL;
static tree_t threads = NULL;

/* Get a process by its PID. The process is not refcounted. If there is no such process, return NULL. */
process_t * process_get(pid_t pid) {
    process_t * process = tree_get(&processes, (tree_key_t) pid);
    if (process)
        ++process->references;
    return process;
}

process_t * process_put(process_t * process) {
    assert(process->references > 0);
    --process->references;
    if (!process->references) {
        process_free(process);
        return NULL;
    } else
        return process;
}

process_t * process_dup(process_t * process) {
    ++process->references;
    return process;
}

void process_add(process_t * process) {
    tree_insert(&processes, process->pid, process);
    ++process->references;
}

void process_del(process_t * process) {
    tree_remove(&processes, process->pid);
    process_put(process);
}

thread_t * thread_get(pid_t pid) {
    thread_t * thread = tree_get(&threads, (tree_key_t) pid);
    if (thread)
        ++thread->references;
    return thread;
}

thread_t * thread_put(thread_t * thread) {
    assert(thread->references > 0);
    --thread->references;
    if (!thread->references) {
        thread_free(thread);
        return NULL;
    } else
        return thread;
}

thread_t * thread_dup(thread_t * thread) {
    ++thread->references;
    return thread;
}

void thread_add(thread_t * thread) {
    tree_insert(&threads, thread->pid, thread);
    tree_insert(&thread->process->threads, thread->pid, thread);
    ++thread->references;
}

void thread_del(thread_t * thread) {
    tree_remove(&threads, thread->pid);
    tree_remove(&thread->process->threads, thread->pid);
    thread_put(thread);
}

void process_iter(void (fn)(process_t *)) {
    TREE_ITER_SAFE(&processes, node) {
        process_t * process = node->val;
        process_dup(process);
        fn(process);
        process_put(process);
    }
}

void thread_iter(process_t * process, void (fn)(thread_t *)) {
    TREE_ITER_SAFE(&process->threads, node) {
        thread_t * thread = node->val;
        thread_dup(thread);
        fn(thread);
        thread_put(thread);
    }
}

thread_t * thread_any_running(process_t * process) {
    TREE_ITER(&process->threads, node) {
        thread_t * thread = node->val;
        if (thread->state.running)
            return thread_dup(thread);
    }
    return NULL;
}

thread_t * thread_any_stopped(process_t * process) {
    TREE_ITER(&process->threads, node) {
        thread_t * thread = node->val;
        if (!thread->state.running && !thread->state.dead)
            return thread_dup(thread);
    }
    return NULL;
}

#ifndef LIST_H_
#define LIST_H_

#include "process.h"
#include "thread.h"

process_t * process_get(pid_t pid) __attribute__((warn_unused_result));
process_t * process_put(process_t * process);
process_t * process_dup(process_t * process);

void process_add(process_t * process);
void process_del(process_t * process);

thread_t * thread_get(pid_t pid) __attribute__((warn_unused_result));
thread_t * thread_put(thread_t * thread);
thread_t * thread_dup(thread_t * thread);

void thread_add(thread_t * thread);
void thread_del(thread_t * thread);

typedef void (process_callback_t)(process_t *);
#define process_lambda(process, ...) ({ void $(process_t * process) { __VA_ARGS__ } $; })

void process_iter(void (fn)(process_t *));
void thread_iter(process_t * process, void (fn)(thread_t *));

thread_t * thread_any_running(process_t * process) __attribute__((warn_unused_result));
thread_t * thread_any_stopped(process_t * process) __attribute__((warn_unused_result));

#endif

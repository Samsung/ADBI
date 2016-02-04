#ifndef LINKER_H
#define LINKER_H

void linker_attach(thread_t * thread);
void linker_detach(thread_t * thread);
void linker_reset(thread_t * process);

void linker_fork(process_t * child, process_t * parent);

void linker_notify(thread_t * thread);

#endif

#ifndef SIGHANDLERS_H
#define SIGHANDLERS_H

extern volatile int signal_child;
extern volatile int signal_io;
extern volatile int signal_quit;
extern volatile int signal_disconnected;
extern volatile int signal_alarm;

int signal_init();

void signal_wait();
void signal_wait_single(int signo, int interruptable);

void signal_block_int(int block);

void signal_reset();

#endif

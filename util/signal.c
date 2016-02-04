#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "signal.h"

static sigset_t signal_signals;

volatile int signal_child;
volatile int signal_io;
volatile int signal_quit;
volatile int signal_disconnected;
volatile int signal_alarm;


static void signal_handler(int sig) {

    if (likely(sig == SIGCHLD)) {
        signal_child += 1;
    } else if (sig == SIGIO) {
        signal_io += 1;
    } else if ((sig == SIGINT) || (sig == SIGTERM)) {
        signal_quit += 1;
    } else if ((sig == SIGPIPE)) {
        signal_disconnected += 1;
    } else if ((sig == SIGALRM)) {
        signal_alarm += 1;
    }
    
    assert(sig != SIGPROF);
    
}

static int signals_block(int signo) {
    sigset_t mask;
    
    if (sigemptyset(&mask) ||
            sigaddset(&mask, signo) ||
            sigprocmask(SIG_BLOCK, &mask, NULL)) {
        return 0;
    }
    return 1;
}

static int signals_unblock(int signo) {
    sigset_t mask;
    
    if (sigemptyset(&mask) ||
            sigaddset(&mask, signo) ||
            sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
        return 0;
    }
    return 1;
}

static int signal_init_single(int signo) {
    int result;
    
    const char * name = str_signal(signo);
    
    struct sigaction action;
    
    if (!signals_block(signo)) {
        fatal("Error blocking signal %s.", name);
        return 0;
    }
    
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    action.sa_flags = 0; //SA_RESTART;
    
    result = sigaction(signo, &action, NULL);
    
    if (result) {
        fatal("Error %s installing %s signal handler.", strerror(errno), name);
        if (!signals_unblock(signo))
            adbi_bug("Error unblocking signal %s.", name);
    } else {
        verbose("Installed %s signal handler.", name);
    }
    
    if (sigdelset(&signal_signals, signo))
        adbi_bug("Error adding signal %s to signal mask.", name)
        
        return result == 0;
}

/* Wait until at least one of the signals handled by us occurs. */
void signal_wait() {
    int res;
    
    assert(!sigismember(&signal_signals, SIGINT));
    
    res = sigsuspend(&signal_signals);
    assert((res == -1) && (errno == EINTR));
}

void signal_reset() {
    for (int signo = 1; signo < 64; ++signo) {
        struct sigaction action;
        sigset_t mask;
        
        /* Restore the original sigaction. */
        sigemptyset(&action.sa_mask);
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
        sigaction(signo, &action, NULL);
        
        /* Unblock the signal. */
        sigemptyset(&mask);
        sigaddset(&mask, signo);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
    }
}

int signal_init() {

    if (sigprocmask(0, NULL, &signal_signals))
        adbi_bug("Error reading signal mask.");
        
    #ifdef ADBI_PROFILING
    sigdelset(&signal_signals, SIGPROF);
    signals_unblock(SIGPROF);
    #endif
    
    /* Block the signals. */
    return signal_init_single(SIGINT) &&
           signal_init_single(SIGTERM) &&
           signal_init_single(SIGIO) &&
           signal_init_single(SIGPIPE) &&
           signal_init_single(SIGCHLD) &&
           signal_init_single(SIGALRM);
           
}

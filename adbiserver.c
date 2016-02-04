#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "communication/communication.h"
#include "communication/protocol.h"

#include "process/process.h"
#include "process/thread.h"

#include "procutil/wait.h"
#include "injectable/injectable.h"

#include "util/signal.h"
#include "util/capabilities.h"

#include "logger.h"

static void cleanup() {
    debug("Cleaning up...");
    process_cleanup();
    comm_cleanup();
    protocol_cleanup();
    injectable_cleanup();
    debug("Cleanup complete.");
}

static void init() {

    int initialized = protocol_init() && caps_init() && signal_init() && comm_init() && injectable_init();
    
    if (!initialized) {
        fatal("Initialization failed.");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
}

static void loop() {

    alarm(10);
    
    while (likely(!signal_quit)) {
    
        #if 0
        {
            /* sanity check */
            void tc(thread_t * thread) {
                assert(thread->references == 2);
            }
            void pc(process_t * process) {
                assert(!tree_empty(&process->threads));
                assert(process->references == 2 + tree_size(&process->threads));
                thread_iter(process, tc);
            }
            process_iter(pc);
        }
        #endif
        
        signal_wait();
        
        if (likely(signal_child)) {
            wait_main();
        }
        
        if (unlikely(signal_io)) {
            comm_handle_io();
            signal_io = 0;
        }
        
        if (unlikely(signal_disconnected)) {
            comm_handle_io();
        }
        
        if (unlikely(signal_alarm)) {
            alarm(10);
            signal_alarm = 0;
        }
        
    }
    
    warning("Exiting.");
}


int main(/* int argc, char * argv[] */) {

    info("Welcome to ADBI server.");
    debug("Built on %s at %s.", __DATE__, __TIME__);
    
    logger_level = LOGGER_DEBUG;
    
    init();
    
    loop();
    cleanup();
    
    return EXIT_SUCCESS;
}

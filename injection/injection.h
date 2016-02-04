#ifndef INJECTION_H_
#define INJECTION_H_

#include "tree.h"
#include "fncall.h"
#include "injectable/injectable.h"

typedef struct process_t process_t;
typedef struct thread_t thread_t;
typedef struct binary_t binary_t;

/* Injectable, which is currently loaded into the memory of a process. */
typedef struct injection_t {

    /* Process where the code is injected. */
    process_t * process;
    
    /* Address in inferior process, where the injectable is loaded */
    address_t address;
    
    /* Injectable loaded into memory */
    const injectable_t * injectable;
    
    /* Reference count to this injectable */
    unsigned int references;
    
} injection_t;

injection_t * injection_get(const process_t * process, const injectable_t * injectable);
const injection_t * injection_get_by_address(const process_t * process, address_t address);

void injection_fork(process_t * child, process_t * parent);
void injection_reset(process_t * process);

void injection_notify_new_thread(thread_t * thread);

injection_t * inject_adbi(thread_t * thread);
bool uninject_adbi(thread_t * thread);

/* Check if the given injection is the ADBI base injection. */
inline static bool injection_is_adbi(injection_t * injection) {
    return injection->injectable == adbi_injectable;
}

/* Get the ADBI base injection for the given process. */
inline static injection_t * injection_get_adbi(const process_t * process) {
    return injection_get(process, adbi_injectable);
}

address_t injection_get_adbi_function_address(const process_t * process, const char * symbol);

void injection_iter(process_t * process, void callback(injection_t *));
void injection_detach(thread_t * thread);

void injection_detach_single(thread_t * thread, injection_t * injection);

#endif

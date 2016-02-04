#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include "tree.h"

typedef struct thread_t thread_t;
typedef struct injectable_t injectable_t;

typedef struct process_t {

    /* Process PID */
    pid_t pid;
    
    tree_t threads;
    tree_t segments;
    tree_t injections;  /* (injectable_t *) -> (address_t) */
    tree_t jumps;
    
    /* Address of linker breakpoint (if installed). */
    struct {
        address_t   bkpt;   /* runtime address */
        insn_t      insn;   /* original instruction */
        insn_kind_t kind;   /* instruction kind */
    } linker;
    
    refcnt_t references;
    
    bool mode32;        /* 32-bit execution mode? */

    bool stabilizing;   /* are we currently stabilizing threads of the process? */
    bool spawned;       /* was the process spawned by us? */
    
} process_t;

void process_free(process_t * process);

process_t * process_spawn(const char * argv[]);
process_t * process_attach(pid_t pid);

void process_kill(process_t * process);
void process_detach(process_t * process);
void process_detach_all();
void process_cleanup();

void process_continue(process_t * process);
void process_stop(process_t * process);

void process_continue_all();
void process_stop_all();

bool process_is_stopped(process_t * process);
bool process_is_running(process_t * process);

thread_t * process_pick_stopped_thread(process_t * process);
thread_t * process_pick_running_thread(process_t * process);

int process_translate_file_to_mem(process_t * process, char * filename, address_t offset,
                                  address_t * address);
int process_translate_mem_to_file(process_t * process, address_t address, char ** filename,
                                  address_t * offset);

int process_is_adbi_address(process_t * process, address_t address);

address_t process_mem_alloc(process_t * process, unsigned int size);
int process_mem_free(process_t * process, address_t address);

process_t * process_forked(process_t * parent, pid_t child_pid);

void process_stabilize(process_t * process);

void process_attach_injectable(process_t * process, const injectable_t * injectable);
void process_detach_injectable(process_t * process, const injectable_t * injectable);

#endif

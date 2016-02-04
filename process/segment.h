#ifndef SEGMENT_H_
#define SEGMENT_H_

#define SEGMENT_READABLE   1
#define SEGMENT_WRITEABLE  2
#define SEGMENT_EXECUTABLE 4
#define SEGMENT_SHARED     8

#include "tree.h"

struct process_t;
struct thread_t;
struct injection_t;

typedef enum segment_state_t {
    SEGMENT_STATE_NEW,          /* Segment was just allocated.      */
    SEGMENT_STATE_OLD,          /* Segment existed at last scan.    */
    SEGMENT_STATE_DELETED,      /* Segment was just deleted.        */
} segment_state_t;

typedef struct segment_t {

    address_t start, end;
    
    struct process_t * process;
    
    char * filename;
    
    address_t offset;
    
    int flags;
    segment_state_t state;
    
    /* injection with tracepoint handlers */
    struct injection_t * injection;
    
    /* tree mapping addresses to tracepoints */
    tree_t tracepoints;
    
    /* address of the trampoline segment */
    address_t trampolines;
    size_t trampolines_size;
    bool trampoline_stolen;
    
} segment_t;

#define segment_is_executable(s) ((s)->flags & SEGMENT_EXECUTABLE)
#define segment_is_readable(s)   ((s)->flags & SEGMENT_READABLE)
#define segment_is_writeable(s)  ((s)->flags & SEGMENT_WRITEABLE)
#define segment_is_shared(s)     ((s)->flags & SEGMENT_SHARED)

typedef struct process_t process_t;

segment_t * segment_get(const process_t * process, address_t address);
const segment_t * segment_get_by_trampoline(const process_t * process, address_t address);

bool segment_check_address(struct process_t * process, address_t address);
bool segment_check_address_is_code(struct process_t * process, address_t address);
bool segment_translate_mem_to_file(struct process_t * process, address_t address,
                                   char ** filename, address_t * offset);
bool segment_translate_file_to_mem(struct process_t * process, const char * filename,
                                   address_t offset, address_t * address);
segment_t * segment_get_by_file_executable(process_t * process, const char * filename);
address_t segment_fo2addr(const segment_t * segment, address_t offset);
address_t segment_addr2fo(const segment_t * segment, address_t address);

void segment_rescan(struct thread_t * process);

void segment_reset(struct process_t * process);
void segment_fork(struct process_t * child, struct process_t * parent);
void segment_detach(struct thread_t * thread);

void segment_iter(struct process_t * process, void callback(segment_t *));
void segment_iter_all(void callback(segment_t *));

static inline bool segment_contains(const segment_t * segment, address_t address) {
    return ((address >= segment->start) && (address < segment->end));
}

bool address_is_trampoline(const process_t * process, address_t address);
bool address_is_injection(const process_t * process, address_t address);
bool address_is_artificial(const process_t * process, address_t address);

void segment_attach_injectable(struct thread_t * thread, const struct injectable_t * injectable);
struct injection_t * segment_detach_injectable(struct thread_t * thread,
        const struct injectable_t * injectable);

bool segment_set_exacutable_all(process_t * process, bool executable);

#endif

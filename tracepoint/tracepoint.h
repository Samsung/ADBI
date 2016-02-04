#ifndef TRACEPOINT_H_
#define TRACEPOINT_H_

#include "process/segment.h"
#include "injectable/injectable.h"
#include "template.h"

struct tracepoint_t {
    /* runtime address */
    address_t address;
    
    /* trampoline runtime address */
    address_t trampoline;
    
    /* handler runtime address */
    address_t handler;
    
    /* instruction and its kind */
    insn_t insn;
    insn_kind_t insn_kind;
    
    /* handler template */
    const template_t * template;
};

typedef struct tracepoint_t tracepoint_t;

void tracepoints_init(thread_t * thread, segment_t * segment);
void tracepoints_gone(thread_t * thread, segment_t * segment);

void tracepoints_reset(segment_t * segment);
void tracepoints_detach(thread_t * thread, segment_t * segment);
void tracepoints_fork(segment_t * child, const segment_t * parent);

bool tracepoints_any_defined(const segment_t * segment, const injectable_t * injectable);

const tracepoint_t * tracepoint_get_by_trampoline_address(const segment_t * segment, address_t address);
const tracepoint_t * tracepoint_get_by_runtime_address(const process_t * process, address_t address);

#endif /* TRACEPOINT_H_ */

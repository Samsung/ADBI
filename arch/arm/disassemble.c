#include "process/process.h"
#include "process/thread.h"
#include "tracepoint/template.h"

#include "tracepoint/tracepoint.h"

void arm_disassemble_handler(thread_t * thread, tracepoint_t * tracepoint, void * code) {

    process_t * process = thread->process;
    insn_t insn;
    offset_t offset = 0;
    address_t base = 0; //tracepoint->trampoline;
    
    int is_patched(size_t bytes) {
        for (offset_t x = 0; x < (offset_t) bytes; ++x) {
            const char * a = ((const char *) code) + x;
            const char * b = ((const char *) tracepoint->template->bindata.data) + offset + x;
            if (*a != *b)
                return 1;
        }
        return 0;
    }
    
    debug("Trampoline for handler at %p for tracepoint at %p in process %s created from template %s:",
          (void *) tracepoint->handler, (void *) tracepoint->address, str_process(process),
          tracepoint->template->name);
          
    while (offset < tracepoint->template->literal_pool) {
        if (tracepoint->insn_kind == INSN_KIND_ARM) {
            /* ARM */
            insn = *((uint32_t *) code);
            debug(" %5lx:  %s %08x   %s",
                  base + offset, is_patched(4) ? "*" : " ",
                  insn, arm_disassemble_extended(insn, INSN_KIND_ARM, base + offset));
            offset += 4;
            code += 4;
        } else {
            /* Thumb */
            uint16_t val = *((uint16_t *) code);
            insn = val;
            
            if (is_thumb2_halfword(insn)) {
                /* 32 bit */
                uint32_t val = *((uint32_t *) code);
                insn = thumb2_swap_halfwords(val);
                
                debug(" %5lx:  %s %04x %04x  %s",
                      base + offset, is_patched(4) ? "*" : " ",
                      thumb2_first_halfowrd(insn), thumb2_last_halfowrd(insn),
                      arm_disassemble_extended(insn, INSN_KIND_THUMB2, base + offset));
                      
                offset += 4;
                code += 4;
            } else {
                /* 16 bit */
                debug(" %5lx:  %s %04x       %s",
                      base + offset, is_patched(4) ? "*" : " ",
                      insn, arm_disassemble_extended(insn, INSN_KIND_THUMB, base + offset));
                offset += 2;
                code += 2;
            }
        }
    }
    
    if (offset < (offset_t) tracepoint->template->bindata.size)
                debug("   -   -   -   -   -   -   -   -   -   -   -   -   -   -");
                
    /* If there's anything left in the buffer, dump it as raw data (.byte). */
    while (offset < (offset_t) tracepoint->template->bindata.size) {
        uint32_t v = *((uint32_t *) code);
        
        const char * addr = str_address(process, v);
        
        if (addr) {
            debug(" %5lx:  %s %08x   %s", base + offset, is_patched(4) ? "*" : " ", v, addr);
        } else {
            debug(" %5lx:  %s %08x   .word %08x", base + offset, is_patched(4) ? "*" : " ", v, v);
        }
        
        offset += 4;
        code += 4;
    }
    
}

#include "tracepoint/jump.h"
#include "process/process.h"
#include "process/thread.h"
#include "procutil/ptrace.h"
#include "process/linker.h"

bool thread_trap(thread_t * thread) {

    pt_regs regs;
    address_t ip, jump_target;
    
    if (unlikely(!thread_get_regs(thread, &regs))) {
        /* Thread died while reading registers. Consider this case as handled. */
        return true;
    }
    
    ip = regs.pc;
    jump_target = jump_get(thread->process, ip);
    
    debug("Thread %s hit a break at %s.", str_thread(thread), str_address(thread->process, ip));
    
    #if 0   /* enable to see full context on each hit */
    dump_thread(thread);
    #endif
    
    if (likely(jump_target)) {
        /* Thread hit a tracepoint. Change the PC and let it continue. */
        if (likely(!thread->process->stabilizing)) {
            debug("Thread %s jumping to %s.", str_thread(thread), str_address(thread->process, jump_target));
            regs.pc = jump_target;
            if (likely(thread_set_regs(thread, &regs))) {
                thread_continue_or_stop(thread, 0);
            }
        } else {
            /* The process is currently stabilizing threads.  The current thread just hit a tracepoint, but we don't
             * change its address to the trampoline, we just leave it stopped.  If we continue the thread now, it
             * will hit the tracepoint again. */
            info("Thread %s is stabilizing, deferring jump to %s.", str_thread(thread),
                 str_address(thread->process, jump_target));
        }
        return true;
    }
    
    /* Linker breakpoint */
    if (likely(ip == thread->process->linker.bkpt)) {
        /* Linker breakpoint */
        linker_notify(thread);
        regs.pc = regs.regs[30];
        if (likely(thread_set_regs(thread, &regs))) {
            thread_continue_or_stop(thread, 0);
        }
        return true;
    }
    
    return false;
}

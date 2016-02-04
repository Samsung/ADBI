#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "injectable/injectable.h"

#include "procutil/procfs.h"
#include "injection/fncall.h"
#include "injection/inject.h"
#include "tracepoint/patch.h"

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"
#include "procutil/mem.h"

#include "procutil/ptrace.h"

/* Start a stopped thread and stop it again immediately. The function replaces
 * the instruction at the PC to a breakpoint instruction, starts the thread and
 * waits for the thread to receive a SIGTRAP (or SIGILL) signal. After the
 * thread stops, the patched instruction is reverted.
 *
 * The function checks if the address pointed by the PC is actually inside an
 * executable memory segment. If it isn't, the function restarts the thread
 * without patching any instructions. The address pointed by the PC is
 * illegal, so the thread should receive a SIGSEGV signal in this case.
 *
 * The function must be called with all threads of the process stopped.
 * Moreover, the given process must still be alive (i.e. not a zombie).
 */
static bool fncall_restop(thread_t * thread) {

    int patch;
    int signo;
    struct pt_regs regs[2];
    
    /* Original instruction. */
    insn_t          insn;
    insn_kind_t     kind;
    address_t       address;
    
    assert(!thread->state.slavemode);
    thread->state.slavemode = true;
    
    info("Restopping thread %s...", str_thread(thread));
    
    /* Save current register values. */
    if (!thread_get_regs(thread, &regs[0]))
        goto fail;
        
    /* Get the current PC. */
    address = instruction_pointer(&regs[0]);
    
    patch = procfs_address_executable(thread, address);
    
    if (patch) {
        /* Read the original instruction. */
        kind = is_thumb_mode(&regs[0]) ? INSN_KIND_THUMB : INSN_KIND_ARM;
        if (!patch_read_insn_detect_kind(thread, address, &insn, &kind))
            goto fail;
            
        verbose("The PC register in thread %s points at address %p, "
                "which contains %s instruction '%s'. It will "
                "be temporary replaced by a breakpoint instruction. The thread "
                "will then be restarted. A SIGILL or SIGTRAP signal should be "
                "received immediately after thread restart.",
                str_thread(thread), (void *) address, insn_kind_str(kind), arm_disassemble(insn, kind));
                
        /* Patch instruction. */
        if (!patch_insn(thread, address, kind, get_breakpoint_insn(kind)))
            goto fail;
            
    } else {
        /* The address pointed by the PC is invalid. Running the thread should
         * cause a SIGSEGV. */
        verbose("The PC register in thread %s points at address %p, "
                "which is invalid (not inside a executable memory segment). "
                "It will be restarted without any patching. A SIGSEGV signal "
                "should be received immediately after thread restart.",
                str_thread(thread), (void *) address);
    }
    
    thread_continue(thread, 0);
    
    /* Wait until the program is stopped by a signal. */
    while (1) {
        thread_wait(thread, false);
        
        if (thread->state.dead)
            goto fail;
            
        if ((thread->state.signo == SIGSEGV) || (thread->state.signo == SIGTRAP) || (thread->state.signo == SIGBUS)) {
            break;
        }
        
        warning("Unexpected signal %s received during restopping of thread %s. The signal will be delivered.",
                str_signal(thread->state.signo), str_thread(thread));
                
        /* Deliver the signal. */
        thread_continue(thread, 0);
    }
    
    signo = thread->state.signo;
    thread->state.signo = 0;
    
    /* The process stopped, read new register values. */
    if (!thread_get_regs(thread, &regs[1]))
        goto fail;
        
    /* Warn about any abnormalities. */
    if (regs[1].ARM_pc != regs[0].ARM_pc) {
        warning("Unexpected change of PC register during restopping of %s.", str_thread(thread));
    }
    if (regs[1].ARM_lr != regs[0].ARM_lr) {
        warning("Unexpected change of LR register during restopping of %s.", str_thread(thread));
    }
    if (regs[1].ARM_sp != regs[0].ARM_sp) {
        warning("Unexpected change of SP register during restopping of %s.", str_thread(thread));
    }
    
    /* Restore old register values. */
    if (!thread_set_regs(thread, &regs[0]))
        goto fail;
        
    if (patch) {
        if ((signo != SIGILL) && (signo != SIGTRAP)) {
            warning("Thread %s was stopped by unexpected signal %s. Ignoring.",
                    str_thread(thread), str_signal(signo));
        }
        /* Revert original instruction */
        patch_insn(thread, address, kind, insn);
    } else {
        if ((signo != SIGSEGV)) {
            warning("%s was stopped by unexpected signal %s. Ignoring.",
                    str_thread(thread), str_signal(signo));
        }
    }
    
    info("Finished restop of %s.", str_thread(thread));
    
    thread->state.slavemode = false;
    if (!thread->state.dead)
        return true;
        
fail:
    assert(thread->state.dead);
    error("Thread %s died during restop operation.", str_thread(thread));
    return false;
}

/* Run given thread until it reaches address stopat. */
bool fncall_runtil(thread_t * thread, address_t stopat) {

    /* TODO: Be more verbose about errors. */
    insn_t          insn;
    insn_kind_t     kind;
    
    assert(!thread->state.slavemode);
    
    info("Running thread %s until it reaches %p (runtil).", str_thread(thread), (void *) stopat);
    
    thread->state.slavemode = true;
    
    kind = (stopat & 0x1) ? INSN_KIND_THUMB : INSN_KIND_ARM;
    if (!patch_read_insn_detect_kind(thread, stopat, &insn, &kind))
        goto fail;
        
    /* Patch instruction. */
    if (!patch_insn(thread, stopat, kind, get_breakpoint_insn(kind)))
        goto fail;
        
    thread_continue(thread, 0);
    
    while (1) {
        thread_wait(thread, false);
        
        if (thread->state.dead)
            goto fail;
            
        if ((thread->state.signo == SIGSEGV) || (thread->state.signo == SIGTRAP) || (thread->state.signo == SIGBUS)) {
            struct pt_regs regs;
            thread_get_regs(thread, &regs);
            if (arch_get_pc(&regs) == (stopat & 0xfffffffe)) {
                /* This is the signal we were waiting for. */
                break;
            }
        }
        
        warning("Unexpected signal %s received during runtil in thread %s. The signal will be delivered.",
                str_signal(thread->state.signo), str_thread(thread));

        dump_thread(thread);
        void callback(segment_t * segment) {
            verbose("%s\n", str_segment(segment));
        }
        segment_iter(thread->process, callback);

        /* Deliver the signal. */
        thread_continue(thread, 0);
    }
    
    /* Revert original instruction */
    if (!patch_insn(thread, stopat, kind, insn))
        goto fail;
        
    info("Thread %s reached the expected address %p.", str_thread(thread), (void *) stopat);

    thread->state.signo = 0;	/* don't deliver the signal we just received */
    thread->state.slavemode = false;
    return true;
    
fail:
    error("Runtil failed in thread %s.  The thread will probably crash.", str_thread(thread));
    thread->state.slavemode = false;
    return false;
    
}

bool fncall_runtil_seg(thread_t * thread, segment_t * segment) {

    /* TODO: Be more verbose about errors. */
    assert(!thread->state.slavemode);
    
    info("Running thread %s until it reaches %p-%p.", str_thread(thread), (void *) segment->start,
         (void *) segment->end);
         
    if (!fncall_call_mprotect(thread, segment->start, segment->end - segment->start,
                              segment->flags & (~PROT_EXEC)))
        goto fail;
        
    thread->state.slavemode = true;
    
    thread_continue(thread, 0);
    
    address_t ip;
    
    while (1) {
        thread_wait(thread, false);
        
        if (thread->state.dead)
            goto fail;
            
        if ((thread->state.signo == SIGSEGV) || (thread->state.signo == SIGTRAP)) {
            struct pt_regs regs;
            thread_get_regs(thread, &regs);
            ip = arch_get_pc(&regs);
            if ((ip >= segment->start) && (ip < segment->end)) {
                /* This is the signal we were waiting for. */
                break;
            }
        }
        
        warning("Unexpected signal %s received during runtil in thread %s. The signal will be delivered.",
                str_signal(thread->state.signo), str_thread(thread));
                
        /* Deliver the signal. */
        thread_continue(thread, 0);
    }
    
    /* Revert original instruction */
    thread->state.slavemode = false;
    
    if (!fncall_call_mprotect(thread, segment->start, segment->end - segment->start, segment->flags))
        return false;
        
    info("Thread %s reached the expected address %p.", str_thread(thread), (void *) ip);
    return true;
    
fail:
    error("Runtil failed in thread %s.  The thread will probably crash.", str_thread(thread));
    thread->state.slavemode = false;
    return false;
    
}

static bool fncall_call(thread_t * thread, unsigned long address, call_context_t * context) {

    bool ret = false;
    int signo;
    unsigned int i;
    struct pt_regs old_regs, regs;
    address_t old_sp;
    int thumb;
    
    #ifdef EXEC_RESTOP
    if (thread->state.execed) {
        if (!fncall_restop(thread))
            goto out;
        thread->state.execed = false;
    }
    #endif
    
    assert(!thread->state.slavemode);
    thread->state.slavemode = true;
    
    /* Detect thumb functions. */
    thumb = address & 0x1;
    
    /* Mask out last bit. */
    address &= ~1;
    
    /* Save current register values. */
    if (!thread_get_regs(thread, &old_regs))
        goto out;
        
    /* Preserve register values. */
    memcpy(&regs, &old_regs, sizeof(struct pt_regs));
    
    /* Set parameter registers. */
    for (i = 0; i < 4; ++i)
        regs.uregs[i] = context->registers[i];
        
    /* Prepare stack. */
    for (i = 0; i < context->stack_elements; ++i) {
        /* ARM uses full descending stacks, this means that:
         *    * the stack grows down
         *    * the sp points to the last element (full cell) on the stack
         */
        regs.ARM_sp -= 4;
        if (!ptrace_mem_write(thread->pid, regs.ARM_sp, context->stack[i])) {
            error("Error pushing elements to stack for function call.");
            goto fail_restore;
        }
    }
    
    /* Save old SP for later checks. */
    old_sp = regs.ARM_sp;
    
    /* Set new PC. */
    regs.ARM_pc = address;
    
    /* Set the correct processor mode. */
    if (thumb) {
        /* Thumb function. */
        regs.ARM_cpsr |= 1 << 5;
    } else {
        /* ARM function. */
        regs.ARM_cpsr &= ~(1 << 5);
    }
    
    /* Set return address to NULL. This way we will be able to easily detect
     * when the function returns, because a segmentation fault will occur. */
    regs.ARM_lr = 0;
    
    if (!thread_set_regs(thread, &regs))
        goto out;
        
    info("Starting %s function at address %s in %s.", thumb ? "Thumb" : "ARM",
         str_address(thread->process, address), str_thread(thread));
         
    thread_continue(thread, 0);
    
    while (1) {
        thread_wait(thread, false);
        
        if (thread->state.dead)
            goto out;
            
        signo = thread->state.signo;
        thread->state.signo = 0;
        
        #ifdef ACCEPT_INJ_TRAPS
        if (signo == SIGTRAP) {
            warning("SIGTRAP during launched function. Ignoring.");
            fncall_print_thread_state(thread);
            ptrace_continue(thread->pid);
            continue;
        }
        #endif
        
        if (!signo)
            adbi_bug("Exec or clone in slave thread.");
            
        if (signo == SIGSTOP) {
            /* Thread was stopped by sigstop, ignore it. */
            thread_continue(thread, 0);
            continue;
        }
        
        break;
    }
    
    if (!thread_get_regs(thread, &regs))
        goto fail_restore;
        
    if ((signo == SIGSEGV) && (regs.ARM_pc == 0)) {
        debug("Function completed returning %li (0x%x).", regs.uregs[0], * ((unsigned int *) &regs.uregs[0]));
        
        /* Read parameter registers. */
        for (i = 0; i < 4; ++i)
            context->registers[i] = regs.uregs[i];
            
        ret = true;
    } else {
        error("Error: ADBI server forced process %s to perform a call to a function at address %s. During its "
              "execution the process received signal %s at address %s. The signal was unexpected, so ADBI server "
              "will now restore the registers to the state before the call and pretend that nothing happened. "
              "Still, the program state may be inconsistent, and the process may crash now.",
              str_thread(thread), str_address(thread->process, address), str_signal(signo),
              str_address(thread->process, regs.ARM_pc));
        dump_thread(thread);
    }
    
    if (old_sp != (address_t) regs.ARM_sp) {
        warning("Called function caused a stack mismatch (expected %p, got %p).",
                (void *) old_sp, (void *) regs.ARM_sp);
    }
    
fail_restore:
    /* Restore old register values. */
    if (!thread_set_regs(thread, &old_regs))
        ret = false;
out:
    thread->state.slavemode = false;
    return ret;
}

/* Find an executable segment of memory in the process memory space, which is at least size bytes large. Return
 * non-zero if found, zero if not found. Save start address at start location. Segments allocated by ADBI are not
 * considered. During this call all threads of the process should be stopped. */
static address_t inject_find_exec_mem(thread_t * thread, unsigned int size) {

    address_t result = 0;
    
    assert(thread);
    assert(size);
    assert(process_is_stopped(thread->process));
    
    void callback(const segment_t * s) {
        if (result)
            return;
            
        if (!segment_is_executable(s))
            return;
            
        if (s->end - s->start < size)
            /* Not enough space. */
            return;
            
        if (injection_get_by_address(thread->process, s->start))
            /* This segment was allocated by ADBI. */
            return;
            
        result = s->start;
    }
    procfs_iter_segments(thread, callback);
    
    return result;
}

/* Inject a function to process memory and launch it.
 *
 * The function allows launching a function even if ADBI has not allocated any memory in the process memory space, where
 * the code could be loaded. This is achieved by finding any segment of memory with the executable bit set and
 * overwriting its contents temporarily with the injected function code. After the function is called, the memory is
 * reverted to original state.
 *
 * Please note that the injected code should be possibly small (so it can fit in most segments). It also must not use
 * any global variables (.data and .bss sections should be empty), because we can only assure execution rights of
 * the segment. Any data should be kept on the stack.
 *
 * Of course, the injected code must be position independent and must not require any kind of dynamic linking or
 * relocation.
 *
 * Returns non-zero on success, zero on failure.
 */
bool fncall_alloc_load_call(thread_t * thread,
                            unsigned char * function_text,
                            size_t size,
                            address_t entry,
                            call_context_t * context) {
                            
    process_t * process = thread->process;
    address_t address;
    unsigned char * original_text = NULL;
    bool ret = false;
    
    /* Find a segment of memory, where the function can fit. */
    if (!(address = inject_find_exec_mem(thread, size))) {
        error("Unable to find %zu bytes of continuous executable memory in process %s.", size, str_process(process));
        goto out;
    }
    
    debug("Found %zu bytes of continuous executable memory in process %s at address %p",
          size, str_process(process), (void *) address);
          
    /* Create backup of the function. */
    original_text = adbi_malloc(size);
    if (mem_read(thread, address, size, original_text) != size) {
        error("Error reading code of process %s in address range %p-%p.",
              str_process(process), (void *) address, (void *)(address + size));
        goto out;
    }
    
    /* Copy the function contents. */
    if (!mem_write(thread, address, size, function_text)) {
        error("Error injecting code into process %s in address range %p-%p.",
              str_process(process), (void *) address, (void *)(address + size));
        goto out;
    } else
        debug("Injected code into process %s in address range %p-%p.",
              str_process(process), (void *) address, (void *)(address + size));
              
    /* Run the function. */
    ret = fncall_call(thread, address + entry, context);
    if (!ret)
        goto out;
        
    /* Restore original memory contents. */
    if (!mem_write(thread, address, size, original_text)) {
        fatal("Error restoring original memory contents of process %s in address range %p-%p.",
              str_process(process), (void *) address, (void *)(address + size));
    } else
        debug("Restored original code of process %s in address range %p-%p.",
              str_process(process), (void *) address, (void *)(address + size));
              
out:
    free(original_text);
    return ret;
}

/* Get the error number of a system call. Return 0 if there was no error. */
int fncall_get_errno(regval_t syscall_ret) {
    /* System calls always return a return code. If it's positive or zero, the
     * call succeeded. If it's negative, there was an error. In this case the
     * value is set to -errno. However, some system calls, e.g. mmap, return
     * negative numbers in case of a success. For this reason we need to check
     * if the return value is negative and in the valid errno range. */
    union {
        regval_t u;
        sregval_t s;
    } ret;
    
    ret.u = syscall_ret;
    
    if ((ret.s < 0) && (ret.s > -256)) {
        return (int) - ret.s;
    } else {
        return 0;
    }
}

/* Align the given size to the page size. */
size_t fncall_align_to_page(size_t size) {
    size_t result;
    static size_t pagesize = 0;
    
    void detect_pagesize() {
        if (likely(pagesize))
            return;
        else {
            unsigned long result = sysconf(_SC_PAGE_SIZE);
            if (result) {
                debug("Detected page size: %lu.", result);
                pagesize = result;
            } else
                adbi_bug("Error detecting memory page size.");
        }
    }
    
    detect_pagesize();
    
    result = (((size - 1) / pagesize) + 1) * pagesize;
    
    return result;
}

static bool fncall_memop(thread_t * thread, const char * fn_name, call_context_t * context,
                         const char ** error_str) {
    injection_t * injection = injection_get_adbi(thread->process);
    offset_t symbol = injectable_get_symbol(adbi_injectable, fn_name);
    
    if (symbol < 0) {
        *error_str = "No such injected function.";
        return false;
    }
    
    address_t fn_addr = injection->address + symbol;
    
    if (fncall_call(thread, fn_addr, context)) {
        int call_errno = fncall_get_errno(context->registers[0]);
        *error_str = call_errno ? strerror(call_errno) : NULL;
        return (call_errno == 0);
    } else {
        *error_str = "Error calling injected function.";
        return false;
    }
}

/* Low level inferior memory mapping function. Calls mmap in the given process with given parameters.
 * This function requires the given thread to be stopped and the ADBI base injectable to be present.  On success, the
 * function returns true and writes the result address to the *res parameter.  On failure, the function returns
 * false.
 */
bool fncall_mmap(thread_t * thread, address_t * res, address_t addr, size_t size, int prot, int flags, int fd, long offset) {
    call_context_t context = call_context_empty;
    const char * error_msg;

    size = fncall_align_to_page(size);
    context.registers[0] = (uint32_t) addr;
    context.registers[1] = (uint32_t) size;
    context.registers[2] = (uint32_t) prot;
    context.registers[3] = (uint32_t) flags;

    stackval_t stack[2] = {(uint32_t) offset, (uint32_t) fd};
    context.stack = stack;
    context.stack_elements = 2;

    verbose("Mapping %u bytes of memory in process %s.", size, str_process(thread->process));

    if (fncall_memop(thread, "adbi_mmap", &context, &error_msg)) {
        *res = context.registers[0];
        info("Mapped memory at %p in process %s.", (void *) *res, str_process(thread->process));
        return true;
    } else {
        error("Error mapping memory in process %s: %s.", str_process(thread->process), error_msg);
        return false;
    }
}

/* Low level inferior memory allocation function. Calls mmap in the given process to allocate size bytes of memory.
 * This function requires the given thread to be stopped and the ADBI base injectable to be present.  On success, the
 * function returns true and writes the result address to the *address parameter.  On failure, the function returns
 * false.
 */
bool fncall_allocate(thread_t * thread, size_t size, address_t * address) {

    call_context_t context = call_context_empty;
    const char * error_msg;
    
    size = fncall_align_to_page(size);
    context.registers[0] = (uint32_t) size;
    
    verbose("Allocating %u bytes of memory in process %s.", size, str_process(thread->process));
    
    if (fncall_memop(thread, "adbi_alloc", &context, &error_msg)) {
        address_t addr = context.registers[0];
        info("Allocated %u bytes of memory at address %p in process %s.",
             size, (void *) addr, str_process(thread->process));
        *address = addr;
        return true;
    } else {
        error("Error allocating %u bytes of memory in process %s: %s.", size,
                str_process(thread->process), error_msg);
        return false;
    }
}

#if 0
/* This function is currently unused. */

/* Low level inferior memory reallocation function. Calls mremap in the given process to allocate size bytes of memory.
 * This function requires the given thread to be stopped and the ADBI base injectable to be present.  On success, the
 * function returns true and updates the address in the *address parameter.  On failure, the function returns
 * false -- in this case the old memory allocation is still present and valid, accessible at the same address.
 */
bool fncall_realloc(thread_t * thread, address_t * address, size_t old_size, size_t new_size) {

    call_context_t context = call_context_empty;
    const char * error_msg;
    address_t old_address = *address;
    
    old_size = fncall_align_to_page(old_size);
    new_size = fncall_align_to_page(new_size);
    
    if (old_size == new_size) {
        /* no need to resize allocation -- treat this as a success */
        return true;
    }
    
    verbose("Resizing allocation at %p in process %s from %u to %u bytes.",
            (void *)(*address), str_process(thread->process), old_size, new_size);
            
    context.registers[0] = *address;
    context.registers[1] = old_size;
    context.registers[2] = new_size;
    
    if (fncall_memop(thread, "adbi_realloc", &context, &error_msg)) {
        address_t new_address = context.registers[0];
        if (new_address == old_address) {
            info("Resized allocation at %p in process %s from %u to %u bytes.",
                 (void *) old_address, str_process(thread->process), old_size, new_size);
        } else {
            info("Resized allocation at %p in process %s from %u to %u bytes. The allocation was moved to address %p.",
                 (void *) old_address, str_process(thread->process), old_size, new_size, (void *) new_address);
            *address = new_address;
        }
        return true;
    } else {
        info("Error resizing allocation at %p in process %s from %u to %u bytes: %s.",
             (void *) old_address, str_process(thread->process), old_size, new_size, error_msg);
        return false;
    }
}

#endif

/* Low level inferior memory freeing function. Calls munmap in the given process to free size bytes of memory at
 * the given address.  This function requires the given thread to be stopped and the ADBI base injectable to be present.
 * Returns success flag. */
bool fncall_free(thread_t * thread, address_t address, size_t size) {

    call_context_t context = call_context_empty;
    const char * error_msg = "";
    
    context.registers[0] = address;
    context.registers[1] = size;
    
    verbose("Freeing %u bytes of memory at address %p in process %s.",
            size, (void *) address, str_process(thread->process));
            
    if (fncall_memop(thread, "adbi_free", &context, &error_msg)) {
        info("Freed %u bytes of memory at address %p in process %s.",
             size, (void *) address, str_process(thread->process));
        return true;
    } else {
        error("Error freeing %u bytes of memory at address %p in process %s: %s.",
              size, (void *) address, str_process(thread->process), error_msg);
        return false;
    }
}

bool fncall_call_adbi(thread_t * thread, address_t address, int arg1, int arg2, int arg3, int arg4,
                      int * ret) {
    call_context_t context = call_context_empty;
    context.registers[0] = arg1;
    context.registers[1] = arg2;
    context.registers[2] = arg3;
    context.registers[3] = arg4;
    if (fncall_call(thread, address, &context)) {
        *ret = context.registers[0];
        return true;
    } else {
        return false;
    }
}

bool fncall_call_mprotect(thread_t * thread, address_t address, size_t size, int prot) {
    int ret;
    const char * whynot;
    address_t fn = injection_get_adbi_function_address(thread->process, "adbi_mprotect");
    assert(fn);
    
    verbose("Calling mprotect(%p, 0x%x, 0x%x) in %s...",
            (void *) address, size, (unsigned int) prot, str_thread(thread));
            
    if (fncall_call_adbi(thread, fn, address, size, prot, 0, &ret)) {
        ret = fncall_get_errno(ret);
        if (ret == 0)
            return true;
        whynot = strerror(ret);
    } else
        whynot = "function call failed";
        
    error("Call to mprotect(%p, 0x%x, 0x%x) failed in %s: %s.",
          (void *) address, size, (unsigned int) prot, str_thread(thread), whynot);
    return false;
}

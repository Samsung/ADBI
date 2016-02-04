#include "segment.h"
#include "process.h"
#include "thread.h"

#include "util/human.h"

#include "procutil/elf.h"
#include "tracepoint/patch.h"

static const char * linker_files[] = {
        "/system/bin/linker"
        "/system/bin/linker64"
        };

static bool is_linker_filename(char * filename) {
    unsigned int i;
    for (i=0; i < sizeof(linker_files)/sizeof(linker_files[0]); ++i)
        if (!strcmp(filename, linker_files[i]))
            return true;
    return false;
}

void linker_attach(thread_t * thread) {
    struct process_t * process = thread->process;
    assert(!process->linker.bkpt);
    thread->process->linker.bkpt = (address_t) elf_get_remote_linker_breakpoint_address(thread);
    
    if (!process->linker.bkpt) {
        info("Process %s has no dynamic linker loaded (or there was an error detecting it).",
             str_process(process));
        return;
    }
    
    /* detect instruction type */
    process->linker.kind = arch_detect_kind_from_unaligned_address(process->mode32, process->linker.bkpt);
    process->linker.bkpt &= ~0x1;
    
    /* check if the address is valid */
    const segment_t * segment = segment_get(process, process->linker.bkpt);
    if (!segment) {
        /* invalid address */
        error("Linker breakpoint address %p is invalid in process %s.",
              (void *) thread->process->linker.bkpt, str_process(thread->process));
        thread->process->linker.bkpt = 0;
        return;
    }
    
    /* make sure this is the linker segment */
    if (!(segment_is_executable(segment) && segment->filename && !is_linker_filename(segment->filename))) {
        /* TODO: The linker binary path should not be hardcoded.  At least not like that.  Maybe we should check if the
         * path matches one of multiple values (ls.so, linker, linux-ld.so, etc.). */
        error("Linker breakpoint address %p does not point to the linker binary in process %s.",
              (void *) thread->process->linker.bkpt, str_process(thread->process));
        thread->process->linker.bkpt = 0;
        return;
    }
    
    if (!patch_read_insn_detect_kind(thread, thread->process->linker.bkpt,
                                     &thread->process->linker.insn,
                                     &thread->process->linker.kind)) {
        warning("Can't read instruction at linker breakpoint address %p in process %s. "
                "No linker breakpoint will be installed.",
                (void *) thread->process->linker.bkpt, str_process(thread->process));
        thread->process->linker.bkpt = 0;
        return;
    }
    
    if (!patch_insn(thread, thread->process->linker.bkpt,
                    thread->process->linker.kind,
                    get_breakpoint_insn(thread->process->linker.kind))) {
        warning("Can't patch instruction at linker breakpoint address %p in process %s. "
                "No linker breakpoint will be installed.",
                (void *) thread->process->linker.bkpt, str_process(thread->process));
        thread->process->linker.bkpt = 0;
        return;
    }
    
    info("Installed linker breakpoint at %p in process %s.", (void *) thread->process->linker.bkpt,
         str_process(thread->process));
}

void linker_detach(thread_t * thread) {
    if (thread->process->linker.bkpt) {
        info("Removing linker breakpoint from process %s.", str_process(thread->process));
        patch_insn(thread, thread->process->linker.bkpt, thread->process->linker.kind, thread->process->linker.insn);
        thread->process->linker.bkpt = 0;
    }
}

void linker_fork(process_t * child, process_t * parent) {
    child->linker = parent->linker;
}

void linker_reset(thread_t * thread) {
    debug("Resetting linker in %s.", str_process(thread->process));
    thread->process->linker.bkpt = 0;
    linker_attach(thread);
}

void linker_notify(thread_t * thread) {
    debug("Linker activity detected in %s.", str_thread(thread));
    segment_rescan(thread);
}

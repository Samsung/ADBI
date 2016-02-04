#include <sys/mman.h>

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"

#include "injectable/injfile.h"
#include "injection/injection.h"

#include "tracepoint.h"
#include "template.h"
#include "patch.h"
#include "jump.h"
#include "procutil/mem.h"

static tracepoint_t * tracepoint_create(thread_t * thread, address_t address, address_t handler_address) {
    insn_kind_t kind = arch_detect_kind_from_unaligned_address(thread->process->mode32, address);
    address &= ~0x1;
    insn_t insn;
    if (!patch_read_insn_detect_kind(thread, address, &insn, &kind)) {
        error("Unable to patch instruction %x kind %d at address %p", insn, kind, (void *) address);
        return NULL;
    }
        
    const template_t * template = template_select(insn, kind);
    
    if (!template) {
        error("No template for instruction %x kind %d at address %p", insn, kind, (void *) address);
        return NULL;
    } else {
        debug("Selected template %s for instruction %x (%s) kind %d at address %p",
                template->name, insn, arch_disassemble(insn, kind), kind, (void *) address);
    }
        
    tracepoint_t * tracepoint = adbi_malloc(sizeof(tracepoint_t));
    
    tracepoint->address = address;
    tracepoint->handler = handler_address;
    tracepoint->insn = insn;
    tracepoint->insn_kind = kind;
    tracepoint->template = template;
    tracepoint->trampoline = 0;
    
    return tracepoint;
}

static void tracepoint_free(tracepoint_t * tracepoint) {
    free(tracepoint);
}

static tracepoint_t * tracepoint_clone(process_t * process, const tracepoint_t * tracepoint, bool install_jump) {
    tracepoint_t * ret = adbi_malloc(sizeof(tracepoint_t));
    memcpy(ret, tracepoint, sizeof(tracepoint_t));
    if (install_jump)
        jump_install(process, tracepoint->address, tracepoint->trampoline);
    return ret;
}

static bool trampoline_free(thread_t * thread, segment_t * segment) {
    if (segment->trampoline_stolen)
        return fncall_call_mprotect(thread, segment->trampolines, segment->trampolines_size, PROT_NONE);

    return fncall_free(thread, segment->trampolines, segment->trampolines_size);
}

/* Free tracepoints installed in a single segment.
 *
 * If unpatch is true, revert original instructions in the segment (if any tracepoints were defined).
 * If free_trampolines is true, free up the trampoline segment (if allocated).
 *
 * If any of these flags is true, the given thread is used for memory access.
 *
 * In other cases, thread is not used for memory access at all and can be NULL.  If thread is not NULL, it must point to
 * a thread of the segment's process.
 */
static void tracepoints_cleanup(
    thread_t * thread,
    segment_t * segment,
    bool unpatch, bool free_trampolines) {
    
    process_t * process = segment->process;
    
    assert((!thread) || (thread->process == process));
    
    if (free_trampolines && segment->trampolines) {
        assert(thread);
        if (!trampoline_free(thread, segment))
            error("Error freeing up trampoline segment at %p in %s.",
                          (void *) segment->trampolines, str_process(thread->process));
    }
    
    /* We didn't free the trampolines, but they should be removed already anyway (because of exit or fork).
     * Forget them. */
    segment->trampolines = 0;
    segment->trampolines_size = 0;
    
    /* Remove tracepoints one by one. */
    tracepoint_t * tracepoint;
    while ((tracepoint = tree_pop(&segment->tracepoints))) {
        if (jump_get(process, tracepoint->address)) {
            /* a jump is installed for the tracepoint -- uninstall it */
            jump_uninstall(process, tracepoint->address);
        }
        if (template_need_return_jump(tracepoint->template)) {
            void callback(address_t from, address_t __attribute__((unused)) to) {
                if (jump_get(process, from)) {
                    /* return jump is installed -- uninstall it */
                    jump_uninstall(process, from);
                }
            }
            template_iter_return_address(tracepoint->address, tracepoint->insn, tracepoint->insn_kind,
                    tracepoint->template, tracepoint->trampoline, callback);
        }
        if (unpatch) {
            /* revert original instruction */
            patch_insn(thread, tracepoint->address, tracepoint->insn_kind, tracepoint->insn);
        }
        tracepoint_free(tracepoint);
    }
    
}

/* Forget installed tracepoints and the trampoline segment without accessing the process memory.
 * This function is called after exec or exit. */
void tracepoints_reset(segment_t * segment) {
    tracepoints_cleanup(NULL, segment, false, false);
}

/* Uninstall tracepoints and remove trampoline segment.  This function is called during detaching. */
void tracepoints_detach(thread_t * thread, segment_t * segment) {
    tracepoints_cleanup(thread, segment, true, true);
}

/* Forget tracepoints and remove trampoline segment.  This function is called when a segment is unloaded (in this case
 * the segment is gone and so are the tracepoints, but the trampoline segment is still there). */
void tracepoints_gone(thread_t * thread, segment_t * segment) {
    tracepoints_cleanup(thread, segment, false, true);
}

bool tracepoints_any_defined(const segment_t * segment, const injectable_t * injectable) {
    assert(strcmp(segment->filename, injectable->injfile->name) == 0);
    INJECTABLE_ITER_TPOINTS(injectable, tp) {
        if (segment_fo2addr(segment, tp->address))
            return true;
    }
    return false;
}

/*
 * Check if given segment is unused. Unused means that segment is private and is non-readable,
 * non-writable and non-executable.
 */
static inline bool segment_is_unused(segment_t * segment) {
    return !segment_is_readable(segment) && !segment_is_writeable(segment) &&
                !segment_is_executable(segment) && !segment_is_shared(segment) &&
                segment->filename == NULL;
}

static inline bool trampoline_mmap(thread_t * thread, address_t address, size_t size, address_t * res) {
    debug("mmaping trampoline segment at 0x%p size 0x%zu", (void *) address, size);
    return fncall_mmap(thread, res, address, size,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
}

static bool tracepoint_iter_free_segments_around(segment_t * segment,
        address_t low, address_t hi, size_t max_size,
        bool (fn)(address_t start, size_t size, segment_t * segment)) {

    const size_t pgsize = fncall_align_to_page(1); // page size
    const address_t addr = fncall_align_to_page(low + (hi - low)/2) - pgsize;
    address_t beoff, aboff;
    bool res = false;

    beoff = addr - segment->start + pgsize;
    aboff = segment->end - addr;
    bool beoff_is_reachable = arch_check_relative_jump(hi, addr - beoff);
    bool aboff_is_reachable = arch_check_relative_jump(low, addr + aboff + pgsize);
    while(beoff_is_reachable || aboff_is_reachable) {
        while (beoff_is_reachable && ((beoff <= aboff) || !aboff_is_reachable)) {
            segment_t * seg = segment_get(segment->process, addr - beoff);
            if (!seg) {
                /* we found gap between segments */
                address_t seg_start = addr - beoff;
                size_t size = pgsize;

                beoff += pgsize;
                beoff_is_reachable = arch_check_relative_jump(hi, addr - beoff);
                while ((size < max_size) &&
                        beoff_is_reachable &&
                        !segment_get(segment->process, addr - beoff)) {

                    size += pgsize;
                    seg_start -= pgsize;

                    beoff += pgsize;
                    beoff_is_reachable = arch_check_relative_jump(hi, addr - beoff);
                }

                res = fn(seg_start, size, NULL);

            } else {
                if (segment_is_unused(seg) && arch_check_relative_jump(hi, seg->start)) {
                    /* unusable segment, probably gap between elf segments */
                    res = fn(0, 0, seg);
                }
                beoff += (seg->end - seg->start);
                beoff_is_reachable = arch_check_relative_jump(hi, addr - beoff);
            }

            if (res)
                return true;
        }
        while (aboff_is_reachable && ((aboff <= beoff) || !beoff_is_reachable)) {
            segment_t * seg = segment_get(segment->process, addr + aboff);
            if (!seg) {
                /* we found gap between segments */
                address_t seg_start = addr + aboff;
                size_t size = pgsize;

                aboff += pgsize;
                aboff_is_reachable = arch_check_relative_jump(low, addr + aboff + pgsize);
                while ((size < max_size) &&
                        aboff_is_reachable &&
                        !segment_get(segment->process, addr - aboff)) {

                    size += pgsize;

                    aboff += pgsize;
                    aboff_is_reachable = arch_check_relative_jump(low, addr - aboff + pgsize);
                }

                res = fn(seg_start, size, NULL);
            } else {
                if (segment_is_unused(seg) && arch_check_relative_jump(low, seg->end)) {
                    /* unusable segment, probably gap between elf segments */
                    res = fn(0, 0, seg);
                }
                aboff += (seg->end - seg->start);
                aboff_is_reachable = arch_check_relative_jump(low, addr + aboff);
            }

            if (res)
                return true;
        }
    }
    return false;
}

/* Function allocates memory for trampoline code */
static bool allocate_trampoline(thread_t * thread, segment_t * segment) {
    assert(thread->process == segment->process);

    address_t tp_low = segment->end;
    address_t tp_high = segment->start;
    TREE_ITER(&segment->tracepoints, node) {
        address_t tp_addr = ((tracepoint_t *) node->val)->address;
        tp_low = tp_addr < tp_low ? tp_addr : tp_low;
        tp_high = tp_high < tp_addr ? tp_addr : tp_high;
    }

    size_t trampolines_size = fncall_align_to_page(segment->trampolines_size);

    /* try to find coherent memory region  */
    bool find_coherent(address_t start, size_t size, segment_t * seg) {
        if (seg && ((seg->end - seg->start) >= trampolines_size)) {
            /* unusable segment, probably gap between elf segments, steal it */
            if (trampoline_mmap(thread, seg->start, trampolines_size, &segment->trampolines)) {
                segment->trampoline_stolen = true;
                return true;
            }
        } else if (trampolines_size <= size) {
            /* we found gap between segments */
            if (trampoline_mmap(thread, start, trampolines_size, &segment->trampolines))
                return true;
        }
        return false;
    }

    if (tracepoint_iter_free_segments_around(segment, tp_low, tp_high, trampolines_size, find_coherent))
        return true;

    warning("Suitable segment for relative jumps not found.");

    return fncall_allocate(thread, segment->trampolines_size, &segment->trampolines);

}

void tracepoints_init(thread_t * thread, segment_t * segment) {
    if (!segment->injection || !segment->injection->injectable->injfile->tpoints) {
        /* The segment has no injection with handlers. */
        return;
    }
    
    segment->trampolines_size = 0;
    
    for (struct injfile_tracepoint_t * tp = segment->injection->injectable->injfile->tpoints; tp->address; ++tp) {
        address_t rt_addr = segment_fo2addr(segment, tp->address);
        address_t handler_addr = segment->injection->address + tp->handler_fn;
        
        if (!rt_addr) {
            /* Tracepoint is outside the segment. */
            continue;
        }
        
        tracepoint_t * tracepoint = tracepoint_create(thread, rt_addr, handler_addr);
        
        if (tracepoint) {
            tree_insert(&segment->tracepoints, rt_addr, tracepoint);
            tracepoint->trampoline = segment->trampolines_size;
            segment->trampolines_size += tracepoint->template->bindata.size;
        } else {
            error("Unable to create tracepoint at %lx for handler at %lx.", rt_addr, handler_addr);
        }
    }
    
    if (tree_empty(&segment->tracepoints)) {
        /* There are no tracepoints for this segment, we don't need a trampoline segment. */
        return;
    }
    
    if (!allocate_trampoline(thread, segment)) {
        /* Error allocating trampoline segment -- forget the tracepoints. */
        segment->trampolines = 0;
        goto rollback;
    }
    
    /* It's time to install the tracepoints. */
    TREE_ITER(&segment->tracepoints, node) {
        tracepoint_t * tracepoint = node->val;
        
        /* Evaluate runtime address of the trampoline. */
        tracepoint->trampoline += segment->trampolines;
        
        /* Make the program jump to the trampoline on tracepoint hit. */
        if (arch_check_relative_jump_kind(tracepoint->insn_kind, tracepoint->address, tracepoint->trampoline)) {
            patch_relative_jump(thread, tracepoint->address, tracepoint->trampoline, tracepoint->insn_kind);
        } else {
            /* Can't jump to trampoline. Use fallback method */
            warning("Can't install relative jump for tracepoint %s to trampoline at %p. Using fallback method.",
                    str_tracepoint(tracepoint), (void *) tracepoint->trampoline);
            patch_breakpoint(thread, tracepoint->address, tracepoint->insn_kind);
            jump_install(thread->process, tracepoint->address, tracepoint->trampoline);
        }
        /* Instantiate the template. */
        template_instance_t * trampoline_code = template_get_handler(tracepoint->template, tracepoint->trampoline,
                tracepoint->address, tracepoint->handler, tracepoint->insn, tracepoint->insn_kind);
                
        assert(trampoline_code);

        if (template_need_return_jump(tracepoint->template)) {
            assert(!thread->process->mode32);
            void callback(address_t from, address_t to) {
                insn_kind_t kind = template_get_template_kind(tracepoint->template);
                offset_t off = from - tracepoint->trampoline;
                insn_t * data_ptr = (insn_t *) (trampoline_code->data + off);
                if (arch_check_relative_jump_kind(kind, from, to)) {
                    *data_ptr = arch_get_relative_jump_insn(kind, from, to);
                    //debug("Installed return relative jump (%x) inside trampoline at offset %lx (%p) to %p:",
                    //        *data_ptr, off, (void *) from, (void *) to);
                } else {
                    warning("Can't install return relative jump for tracepoint %s trampoline at %p. Using fallback method.",
                            str_tracepoint(tracepoint), (void *) tracepoint->trampoline);
                    *data_ptr = get_breakpoint_insn(kind);
                    jump_install(thread->process, from, to);
                }
            }
            template_iter_return_address(tracepoint->address, tracepoint->insn, tracepoint->insn_kind,
                    tracepoint->template, tracepoint->trampoline, callback);
        }

        /* Copy the trampoline into the process. */
        if (trampoline_code->size != mem_write(thread, tracepoint->trampoline, trampoline_code->size,
                                               trampoline_code->data))
            goto rollback;

        arch_disassemble_handler(thread, tracepoint, trampoline_code->data);
        template_instance_free(trampoline_code);
    }
    return;
    
rollback:
    /* Try to revert all changes. */
    tracepoints_cleanup(thread, segment, true, true);
}

void tracepoints_fork(segment_t * child, const segment_t * parent) {
    if (parent->injection) {
        assert(parent->injection->injectable);
        child->injection = injection_get(child->process, parent->injection->injectable);
        assert(child->injection);
        ++child->injection->references;
    }
    child->trampolines = parent->trampolines;
    child->trampolines_size = parent->trampolines_size;
    child->trampoline_stolen = parent->trampoline_stolen;
    
    /* Clone tracepoints. */
    TREE_ITER(&parent->tracepoints, node) {
        bool install_jump = false;
        tracepoint_t * tp = (tracepoint_t *) node->val;
        if (tree_get(&parent->process->jumps, tp->address))
            install_jump = true;

        void callback(address_t from, address_t to) {
            if (tree_get(&parent->process->jumps, from))
                jump_install(child->process, from, to);
        }
        template_iter_return_address(tp->address, tp->insn, tp->insn_kind, tp->template, tp->trampoline, callback);

        tree_insert(&child->tracepoints, node->key, tracepoint_clone(child->process, node->val, install_jump));
    }
}

const tracepoint_t * tracepoint_get_by_trampoline_address(const segment_t * segment, address_t address) {
    TREE_ITER(&segment->tracepoints, node) {
        tracepoint_t * tracepoint = node->val;
        address_t low = tracepoint->trampoline;
        address_t high = low + tracepoint->template->bindata.size;
        if ((low <= address) && (address < high))
                    return tracepoint;
    }
            return NULL;
}

const tracepoint_t * tracepoint_get_by_runtime_address(const process_t * process, address_t address) {
    segment_t * segment = segment_get(process, address);
    if (!segment)
        return NULL;
    return tree_get(&segment->tracepoints, address);
}

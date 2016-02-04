#include "tree.h"

#include "process/process.h"

#include "tracepoint/tracepoint.h"
#include "injectable/injectable.h"

#include "procutil/mem.h"

#include "injection.h"
#include "fncall.h"

static injection_t * injection_create(process_t * process, const injectable_t * injectable,
                                      address_t address) {
    injection_t * injection = adbi_malloc(sizeof(injection_t));
    assert(process);
    assert(injectable);
    
    injection->references = 0;
    injection->process = process;
    injection->address = address;
    injection->injectable = injectable;
    ((injectable_t *) injection->injectable)->references++;
    
    tree_insert(&process->injections, (tree_key_t) injection->injectable, injection);
    
    return injection;
}

static void injection_free(injection_t * injection) {
    tree_remove(&injection->process->injections, (tree_key_t) injection->injectable);
    ((injectable_t *) injection->injectable)->references--;
    free(injection);
}

injection_t * injection_get(const process_t * process, const injectable_t * injectable) {
    return tree_get(&process->injections, (tree_key_t) injectable);
}

/* Check if the given injection contains the given address. */
static bool injection_contains(const injection_t * injection, address_t address) {
    address_t start = injection->address;
    address_t end = start + injection->injectable->injfile->code_size;
    return (address >= start) && (address < end);
}

/* Return the injection in the process, which contains the given address. */
const injection_t * injection_get_by_address(const process_t * process, address_t address) {
    TREE_ITER(&process->injections, node) {
        const injection_t * injection = node->val;
        if (injection_contains(injection, address))
            return injection;
    }
    return NULL;
}

void injection_iter(process_t * process, void callback(injection_t *)) {
    TREE_ITER(&process->injections, node) {
        injection_t * injection = node->val;
        callback(injection);
    }
}

/******************************************************************************/

static address_t injection_relocate_address(const injection_t * injection, offset_t offset) {
    if (offset < 0) {
        return 0;
    } else {
        return injection->address + offset;
    }
}

static address_t injection_get_adbi_symbol_address(const injection_t * injection, const char * symbol) {
    offset_t offset = injectable_get_symbol(injection->injectable, symbol);
    return injection_relocate_address(injection, offset);
}

static address_t injection_get_exported_symbol_address(const injection_t * injection, const char * symbol) {
    offset_t offset = injectable_get_exported_symbol(injection->injectable, symbol);
    return injection_relocate_address(injection, offset);
}

address_t injection_get_adbi_function_address(const process_t * process, const char * symbol) {
    const injection_t * injection = injection_get_adbi(process);
    return injection ? injection_get_adbi_symbol_address(injection, symbol) : 0;
}

static bool injection_init(thread_t * thread, injection_t * injection) {
    address_t entry = injection_get_adbi_symbol_address(injection, "entry");
    
    if (!entry) {
        warning("Injectable %s has no initialization routine.  Skipping initialization in process %s.",
                str_injectable(injection->injectable), str_process(thread->process));
        return true;
    }
    
    int ret;
    int pid  = (int) thread->pid;
    int tgid = (int) thread->process->pid;
    info("Initializing injectable %s in process %s.",
         str_injectable(injection->injectable), str_process(thread->process));
    if (fncall_call_adbi(thread, entry, pid, tgid, 0, 0, &ret)) {
        info("Injection %s initialized successfully in process %s.",
             str_injection(injection), str_process(thread->process));
        return true;
    } else {
        error("Injection %s failed to initialize in process %s: %s.",
              str_injection(injection), str_process(thread->process),
              strerror(ret));
        return false;
    }
}

static void injection_call_new_process_handlers(thread_t * thread) {
    process_t * process = thread->process;
    TREE_ITER(&process->injections, node) {
        injection_t * injection = node->val;
        address_t address = injection_get_adbi_symbol_address(injection, "new_process");
        if (address) {
            int ret, pid = (int) process->pid;
            debug("Calling new process handler from injectable %s in process %s.",
                    str_injectable(injection->injectable), str_process(process));
            if (fncall_call_adbi(thread, address, pid, 0, 0, 0, &ret)) {
                debug("Successfully called new process handler from injection %s in process %s.",
                        str_injection(injection), str_process(process));
                if (ret)
                    warning("New process handler from injection %s in process %s returns: %s",
                            str_injection(injection), str_process(process), strerror(ret));
            } else {
                error("Calling new process handler from injection %s failed in process %s: %s.",
                        str_injection(injection), str_process(process), strerror(ret));
            }
        }
    }
}

static void injection_call_new_thread_handlers(thread_t * thread) {
    TREE_ITER(&thread->process->injections, node) {
        injection_t * injection = node->val;
        address_t address = injection_get_adbi_symbol_address(injection, "new_thread");
        if (address) {
            int ret;
            int pid  = (int) thread->pid;
            int tgid = (int) thread->process->pid;
            debug("Calling new thread handler from injectable %s in thread %s.",
                    str_injectable(injection->injectable), str_thread(thread));
            if (fncall_call_adbi(thread, address, pid, tgid, 0, 0, &ret)) {
                debug("Successfully called new thread handler from injection %s in thread %s.",
                        str_injection(injection), str_thread(thread));
                if (ret)
                    warning("New thread handler from injection %s in thread %s returns: %s",
                            str_injection(injection), str_thread(thread), strerror(ret));
            } else {
                error("Calling new thread handler from injection %s failed in thread %s: %s.",
                        str_injection(injection), str_thread(thread), strerror(ret));
            }
        }
    }
}

void injection_notify_new_thread(thread_t * thread) {
    assert(thread->state.running == false);

    if (thread->pid == thread->process->pid) {
        node_t * node = tree_iter_start(&thread->process->threads);
        assert(node->val == thread);
        assert(tree_iter_next(node) == NULL);

        injection_call_new_process_handlers(thread);
    } else {
        injection_call_new_thread_handlers(thread);
    }

    thread->notified = true;
}

static address_t injection_find_export(const process_t * process, const char * symbol) {
    TREE_ITER(&process->injections, node) {
        injection_t * injection = node->val;
        address_t addr = injection_get_exported_symbol_address(injection, symbol);
        if (addr)
            return addr;
    }
    return 0;
}

static bool injection_dlink(thread_t * thread, injection_t * injection) {
    bool success = true;
    info("Dynamic linking injection %s.", str_injection(injection));
    INJECTABLE_ITER_IMPORTS(injection->injectable, import) {
        address_t rt_addr = injection_find_export(thread->process, import->name);
        if (rt_addr) {
            verbose("Resolved external %s in injection %s -- runtime address is %p.",
                    import->name, str_injection(injection), (void *) rt_addr);
            address_t import_addr = injection_relocate_address(injection, import->offset);
            assert(import_addr);
            if (thread->process->mode32)
                import_addr += 4;
            else
                import_addr += 8;
            if (mem_write(thread, import_addr, sizeof(address_t), &rt_addr) != sizeof(address_t)) {
                error("Error inserting runtime address %p during dynamic linking of %s.",
                      (void *) import_addr, str_injection(injection));
                success = false;
            }
        } else {
            error("Unresolved external %s in injection %s.", import->name, str_injection(injection));
            success = false;
        }
    }
    info("Dynamic linking injection %s %s.", str_injection(injection), success ? "succeeded" : "failed");
    return success;
}

/******************************************************************************/

/* Remove the given injectable from the virtual memory of the given process. Use this function only if the injected
 * segment really exists. If the segment was deleted (because of exiting or execing), just free the injection. */
static void uninject(thread_t * thread, injection_t * injection) {
    assert(!injection_is_adbi(injection));
    assert(thread->process == injection->process);
    assert(injection->references == 0);
    debug("Removing %s from process %s.", str_injection(injection), str_process(injection->process));
    fncall_free(thread, injection->address, injection->injectable->injfile->code_size);
    injection_free(injection);
}

/******************************************************************************/

/* Inject the given injectable into the given process virtual memory space.
 * Return the new injection or NULL on error. */
static injection_t * inject(thread_t * thread, const injectable_t * injectable) {

    injection_t * injection = NULL;
    address_t address = 0;
    
    assert(injectable != adbi_injectable);
    
    if (!fncall_allocate(thread, injectable->injfile->code_size, &address)) {
        /* Error message printed by called function. */
        return NULL;
    }
    
    if (injectable->injfile->code_size != mem_write(thread, address, injectable->injfile->code_size,
            injectable->injfile->code)) {
        error("Error injecting %s data into process %s.", str_injectable(injectable), str_process(thread->process));
        
        /* If the thread is not dead, try to free up the memory. */
        if (!thread->state.dead)
            fncall_free(thread, address, injectable->injfile->code_size);
            
        goto out;
    }
    
    injection = injection_create(thread->process, injectable, address);
    info("Injected %s into process %s.", str_injectable(injectable), str_process(thread->process));
    
    /* Perform dynamic linking. */
    if (!injection_dlink(thread, injection))
        goto error;
        
    /* Call initialization function. */
    if (!injection_init(thread, injection)) {
        goto error;
    } else {
        goto out;
    }
    
error:
    if (injection) {
        uninject(thread, injection);
        injection = NULL;
    }
out:
    return injection;
}

injection_t * inject_adbi(thread_t * thread) {
    injection_t * injection = NULL;
    const injectable_t * mmap_inj = injectable_get_library("adbi_mmap");
    offset_t mmap_sym = injectable_get_symbol(mmap_inj, "adbi_mmap");
    call_context_t context = call_context_empty;
    address_t address;
    int call_errno;
    
    info("Installing ADBI base injection in process %s.", str_process(thread->process));
    
    assert(process_is_stopped(thread->process));
    
    assert(adbi_injectable);
    assert(mmap_sym >= 0);

    /*pt_regs regs;
    if (!thread_get_regs(thread, &regs))
        return NULL;

    thread->process->mode32 = arch_is_mode32(regs);*/

    assert(!injection_get(thread->process, adbi_injectable));
    
    context.registers[0] = fncall_align_to_page(adbi_injectable->injfile->code_size);
    
    if (!fncall_alloc_load_call(thread, mmap_inj->injfile->code, mmap_inj->injfile->code_size, mmap_sym,
                                &context))
        return NULL;
        
    address = context.registers[0];
    call_errno = fncall_get_errno(context.registers[0]);
    if (call_errno) {
        /* the memory, where the adbi_mmap function was loaded is already restored (if the thread is alive) */
        error("Error allocating memory for ADBI segment in process %s: %s.",
              str_process(thread->process), strerror(call_errno));
        return NULL;
    }
    
    /* initialize allocated memory */
    if (mem_write(thread, address, adbi_injectable->injfile->code_size,
                  adbi_injectable->injfile->code) != adbi_injectable->injfile->code_size)
        return NULL;
        
    injection = injection_create(thread->process, adbi_injectable, address);
    ++injection->references;    /* prevent manual unloading */
    
    if (!injection_init(thread, injection)) {
        uninject_adbi(thread);
        return NULL;
    } else {
        return injection;
    }
}

static int injection_exit(thread_t * thread, injection_t * injection) {
    address_t exit = injection_get_adbi_symbol_address(injection, "exit");
    if (!exit)
        return 0;

    int ret = 0;
    process_t * process = thread->process;
    int pid = (int) process->pid;

    debug("Calling exit handler for injectable %s in process %s.",
            str_injectable(injection->injectable), str_process(process));
    if (fncall_call_adbi(thread, exit, pid, 0, 0, 0, &ret)) {
        debug("Successfully called exit handler from injection %s in process %s.",
                str_injection(injection), str_process(process));
        if (ret)
           warning("Exit handler from injection %s in process %s returns: %s",
                    str_injection(injection), str_process(process), strerror(ret));
    } else {
        error("Calling exit handler from injection %s failed in process %s: %s.",
                str_injection(injection), str_process(process), strerror(ret));
    }
    return ret;
}

bool uninject_adbi(thread_t * thread) {
    injection_t * injection = injection_get_adbi(thread->process);
    const injectable_t * munmap_inj = injectable_get_library("adbi_munmap");
    offset_t munmap_sym = injectable_get_symbol(munmap_inj, "adbi_munmap");
    call_context_t context = call_context_empty;
    int call_errno;
    
    info("Removing ADBI base injection from process %s.", str_process(thread->process));
    
    assert(process_is_stopped(thread->process));
    
    assert(adbi_injectable && munmap_inj && munmap_sym);
    assert(injection);
    
    injection_exit(thread, injection);

    context.registers[0] = injection->address;
    context.registers[1] = fncall_align_to_page(adbi_injectable->injfile->code_size);;
    
    if (!fncall_alloc_load_call(thread, munmap_inj->injfile->code, munmap_inj->injfile->code_size, munmap_sym,
                                &context))
        return false;
        
    call_errno = fncall_get_errno(context.registers[0]);
    if (call_errno) {
        /* the memory, where the adbi_munmap function was loaded is already restored (if the thread is alive) */
        error("Error freeing ADBI segment in process %s: %s.", str_process(thread->process), strerror(call_errno));
        return NULL;
    }
    
    --injection->references;
    assert(!injection->references);
    injection_free(injection);
    return true;
}

/**********************************************************************************************************************/

/* Forget all injected code information (without uninjecting). This should be used when a process exits or calls
 * exec. */
void injection_reset(process_t * process) {
    /* Forget all injections. */
    debug("Resetting injections in %s.", str_process(process));
    
    TREE_ITER_SAFE(&process->injections, node) {
        /* This will handle the adbi injection correctly as well. */
        injection_free((injection_t *) node->val);
    }
    
}

void injection_fork(process_t * child, process_t * parent) {
    debug("Cloning injections from %s to %s.", str_process(parent), str_process(child));
    TREE_ITER(&parent->injections, node) {
        injection_t * injection = node->val;
        injection_t * child_injection = injection_create(child, injection->injectable, injection->address);
        if (injection_is_adbi(child_injection))
            /* this is the adbi injection -- mark it as referenced */
            ++child_injection->references;
        else if (injectable_is_library(child_injection->injectable))
            child_injection->references = injection->references;
    }
}

void uninject_dependencies(thread_t * thread, injection_t * injection) {
    void dep_uninject(const injectable_t * injectable) {
        injection_t * dep_injection = injection_get(thread->process, injectable);
        --dep_injection->references;
        if (!dep_injection->references && !injection_is_adbi(dep_injection))
            injection_detach_single(thread, dep_injection);
    }
    injectable_iter_dependencies(injection->injectable, dep_uninject, NULL);
}

void injection_detach_single(thread_t * thread, injection_t * injection) {
    assert(injection->process == thread->process);
    assert(process_is_stopped(thread->process));
    assert(!injection_is_adbi(injection));
    assert(injection->references == 0);

    injection_exit(thread, injection);
    uninject_dependencies(thread, injection);
    uninject(thread, injection);
}

void injection_detach(thread_t * thread) {
    debug("Removing injections from %s.", str_process(thread->process));
    TREE_ITER_SAFE(&thread->process->injections, node) {
        injection_t * injection = node->val;
        if (!injection_is_adbi(injection) && !injectable_is_library(injection->injectable))
            injection_detach_single(thread, injection);
    }
    uninject_adbi(thread);
}

/**********************************************************************************************************************/

static bool inject_dependencies(thread_t * thread, const injectable_t * injectable) {
    bool success = true;

    /* check dependencies */
    void unresolved(const injectable_t * inj, const injfile_symbol_t * symbol) {
        success = false;
        error("Unresolved import %s in injectable %s.", symbol->name, str_injectable(inj));
    }
    injectable_iter_dependencies(injectable, NULL, unresolved);

    if (success) {
        /* inject dependencies */
        void dependency(const injectable_t * inj) {
            injection_t * injection = injection_get(thread->process, inj);
            if (!injection)
                injection = inject(thread, inj);
            else
                /* The injectable is already loaded into the process. */
                assert(injection->injectable == inj);

            ++injection->references;
        }
        injectable_iter_dependencies(injectable, dependency, NULL);
    }
    return success;
}

void injections_init(thread_t * thread, segment_t * segment) {
    assert(thread->process == segment->process);

    if (segment->injection) {
        /* The injectable is already loaded and we already have a handler segment. */
        assert(injection_get(thread->process, injectable_get_binding(segment->filename)));
        return;
    }
    
    if (!segment->filename) {
        /* Do not inject code into unnamed memory mappings. */
        return;
    }
    
    if (!segment_is_executable(segment) || segment_is_writeable(segment)) {
        /* Don't inject any code if the segment is not executable and is writable.
         * TODO: We should check if given segment contains .text section */
        return;
    }
    
    const injectable_t * injectable = injectable_get_binding(segment->filename);
    
    if (!injectable) {
        /* There are no bindings for this segment */
        return;
    }
    
    if (!tracepoints_any_defined(segment, injectable)) {
        /* None of the tracepoints defined by the injectable is mapped in the segment. */
        return;
    }
    
    //debug("Initializing injection in segment: %s", str_segment(segment));

    injection_t * injection = injection_get(thread->process, injectable);
    
    if (!injection) {
        /* Load injectable dependencies */
        if (inject_dependencies(thread, injectable))
            /* Load the injectable into the process. */
            injection = inject(thread, injectable);
    } else {
        /* The injectable is already loaded into the process. */
        assert(injection->injectable == injectable);
    }
    
    if (!injection) {
        /* Injecting failed. */
        return;
    }
    
    segment->injection = injection;
    ++segment->injection->references;
}





#include <sys/mman.h>

#include "segment.h"
#include "process.h"
#include "thread.h"
#include "list.h"

#include "util/human.h"
#include "procutil/procfs.h"

#include "injection/inject.h"
#include "tracepoint/patch.h"
#include "tracepoint/tracepoint.h"

static void segment_release_injection(segment_t * segment) {
    assert(tree_empty(&segment->tracepoints));
    assert(!segment->trampolines);
    if (segment->injection) {
        --segment->injection->references;
        segment->injection = NULL;
    }
}

static void segment_free(segment_t * segment) {
    assert(tree_empty(&segment->tracepoints));
    assert(!segment->trampolines);
    tree_remove(&segment->process->segments, segment->start);
    segment_release_injection(segment);
    free(segment->filename);
    free(segment);
}

static segment_t * segment_clone(process_t * process, const segment_t * segment) {

    segment_t * clone = adbi_malloc(sizeof(segment_t));
    
    memcpy(clone, segment, sizeof(segment_t));
    
    clone->process = process;
    clone->filename = segment->filename ? strdup(segment->filename) : NULL;
    clone->state = SEGMENT_STATE_NEW;
    
    clone->tracepoints = NULL;
    clone->injection = NULL;
    clone->trampolines = 0;
    clone->trampolines_size = 0;
    clone->trampoline_stolen = false;
    
    tree_insert(&process->segments, clone->start, clone);
    return clone;
}

segment_t * segment_get(const process_t * process, address_t address) {
    segment_t * segment = tree_get_le(&process->segments, (tree_key_t) address);
    
    if (!segment)
        return NULL;
        
    if (address >= segment->end)
        return NULL;
        
    return segment;
}

/******************************************************************************/

void segment_iter(struct process_t * process, void callback(segment_t *)) {
    TREE_ITER(&process->segments, node) {
        segment_t * segment = node->val;
        callback(segment);
    }
}

void segment_iter_all(void callback(segment_t *)) {
    void inner(struct process_t * process) {
        segment_iter(process, callback);
    }
    process_iter(inner);
}

/**********************************************************************************************************************/

bool segment_check_address_is_code(process_t * process, address_t address) {
    segment_t * segment = segment_get(process, address);
    if (!segment)
        return 0;
    else
        return segment_is_executable(segment);
}

bool segment_check_address(process_t * process, address_t address) {
    return segment_get(process, address) != NULL;
}

/**********************************************************************************************************************/

address_t segment_fo2addr(const segment_t * segment, address_t offset) {
    address_t result;
    assert(segment->filename);
    result = segment->start + offset - segment->offset;
    if ((result < segment->start) || (result >= segment->end))
        return 0;
    else
        return result;
}

address_t segment_addr2fo(const segment_t * segment, address_t address) {
    assert(segment_contains(segment, address));
    return address - segment->start + segment->offset;
}

/**********************************************************************************************************************/

/* Translate a virtual address in a process memory space to a filename and
 * offset in a file. */
bool segment_translate_mem_to_file(process_t * process, address_t address, char ** filename,
                                   address_t * offset) {
                                   
    segment_t * segment = segment_get(process, address);
    
    if (!segment)
        return false;
        
    if (!segment->filename)
        return false;
        
    if (filename) {
        *filename = segment->filename;
    }
    
    if (offset) {
        *offset = segment_addr2fo(segment, address);
    }
    
    return true;
}

/* Get the first executable segment mapped to given filename. */
segment_t * segment_get_by_file_executable(process_t * process, const char * filename) {

    node_t * node;
    
    for (node = tree_iter_start(&process->segments);
            node;
            node = tree_iter_next(node)) {
        segment_t * segment = (segment_t *) node->val;
        if (!segment_is_executable(segment))
            continue;
        if (!segment->filename)
            /* Segment is not mapped to a file. */
            continue;
        if ((strcmp(segment->filename, filename) != 0))
            /* File name doesn't match. */
            continue;
        return segment;
    }
    
    return NULL;
}

static segment_t * segment_get_by_file(
    process_t * process,
    const char * filename, address_t offset,
    address_t * virtual_address) {
    
    node_t * node;
    segment_t * segment;
    
    assert(process);
    assert(filename);
    assert(strlen(filename) < MAX_PATH);
    
    for (node = tree_iter_start(&process->segments);
            node;
            node = tree_iter_next(node)) {
            
        segment = (segment_t *) node->val;
        
        address_t offset_in_segment, address;
        
        if (!segment->filename)
            /* Segment is not mapped to a file. */
            continue;
            
        if ((strcmp(segment->filename, filename) != 0))
            /* File name doesn't match. */
            continue;
            
        if (offset < segment->offset)
            continue;
            
        offset_in_segment = offset - segment->offset;
        address = segment->start + offset_in_segment;
        
        if (address >= segment->end)
            /* Out of range. */
            continue;
            
        /* Found. */
        if (virtual_address)
            *virtual_address = address;
            
        return segment;
    }
    
    return NULL;
}

/* Translate an address in a file to an address in the process memory space. */
bool segment_translate_file_to_mem(process_t * process, const char * filename, address_t offset,
                                   address_t * address) {
    return (segment_get_by_file(process, filename, offset, address) != NULL);
}

/******************************************************************************/

/* Return non-zero if segments are equal. */
static int segment_equal(const segment_t * a, const segment_t * b) {
    if ((a->start != b->start) || (a->end != b->end)) {
        /* Different address ranges. */
        return 0;
    }
    
    if ((a->filename != NULL) != (b->filename != NULL)) {
        /* One of the segments is mapped to a file, the other is not. */
        return 0;
    }
    
    if (a->filename) {
        return (a->offset == b->offset) &&
               (strcmp(a->filename, b->filename) == 0);
    } else {
        return 1;
    }
}

void segment_rescan(thread_t * thread) {

    bool again = false;
    
    void notify(const segment_t * segment) {
        segment_t * old_segment = tree_get(&thread->process->segments, segment->start);
        
        if (likely(old_segment != NULL)) {
            /* A segment at the address did already exist. */
            
            if (segment_equal(segment, old_segment)) {
                /* This is the same segment as last time.  Mark it as existing. */
                old_segment->state = SEGMENT_STATE_OLD;
            } else {
                /* The segment changed!  Don't mark the old one as existing, just skip it.  It will be recognized as
                 * deleted.  The new segment will be added on the next iteration.  */
                again = true;
            }
            
        } else {
        
            if (address_is_artificial(thread->process, segment->start)) {
                /* This is an artificial segment, created by ADBI. */
                return;
            }
            
            /* This segment is new.  Initially do not set any trace information -- it will be created if necessary
             * after discovering all segment changes. */
            segment_t * new_segment = segment_clone(thread->process, segment);
            new_segment->state = SEGMENT_STATE_NEW;
        }
    }
    
    
    do {
        again = false;
        
        TREE_ITER(&thread->process->segments, node) {
            segment_t * segment = node->val;
            segment->state = SEGMENT_STATE_DELETED;
        }
        
        procfs_iter_segments(thread, notify);
        
        TREE_ITER_SAFE(&thread->process->segments, node) {
            segment_t * segment = node->val;
            switch (segment->state) {
                case SEGMENT_STATE_NEW:
                    //debug("Segment created in process %d: %s", thread->process->pid, str_segment(segment));
                    injections_init(thread, segment);
                    tracepoints_init(thread, segment);
                    segment->state = SEGMENT_STATE_OLD;
                    break;
                    
                case SEGMENT_STATE_DELETED:
                    //debug("Segment deleted in process %u: %s", thread->process->pid, str_segment(segment));
                    tracepoints_gone(thread, segment);
                    segment_free(segment);
                    break;
                    
                case SEGMENT_STATE_OLD:
                    break;
                    
                default:
                    assert(0);
            }
        }
        
    } while (again);
    
}


/* Clone all memory information of process src to process dst. This function
 * can only be called after src has forked and created dst. */
void segment_fork(process_t * child, process_t * parent) {
    TREE_ITER(&parent->segments, node) {
        segment_t * segment = node->val;
        segment_t * clone = segment_clone(child, segment);
        tracepoints_fork(clone, segment);
    }
}

void segment_reset(process_t * process) {
    debug("Resetting segments in %s.", str_process(process));
    TREE_ITER_SAFE(&process->segments, node) {
        segment_t * segment = node->val;
        tracepoints_reset(segment);
        segment_free(segment);
    }
}

void segment_detach(thread_t * thread) {
    TREE_ITER_SAFE(&thread->process->segments, node) {
        segment_t * segment = node->val;
        tracepoints_detach(thread, segment);
        segment_free(segment);
    }
}

/* Remove references to the injection, which matches the given injectable.  This includes removing installed tracepoints
 * and releasing trampoline segments.  Return the injection or NULL if the injectable was not loaded into the process.
 */
injection_t * segment_detach_injectable(thread_t * thread, const injectable_t * injectable) {
    injection_t * injection = injection_get(thread->process, injectable);
    if (!injection)
        return NULL;
    TREE_ITER(&thread->process->segments, node) {
        segment_t * segment = node->val;
        if (segment->injection != injection) {
            /* Segment has a different injectable assigned. */
            continue;
        }
        /* Uninstall tracepoints */
        tracepoints_detach(thread, segment);
        /* Detach the injection */
        segment_release_injection(segment);
    }
    return injection;
}

/* Inject the given injectable into the thread's process and insert tracepoints. If the injectable is already injected
 * or it does not match any segment, do nothing. */
void segment_attach_injectable(thread_t * thread, const injectable_t * injectable) {
    UNUSED(injectable);
    TREE_ITER(&thread->process->segments, node) {
        segment_t * segment = node->val;
        if (!segment->injection) {
            injections_init(thread, segment);
            tracepoints_init(thread, segment);
        }
    }
}

/* Return segment with a trampoline segment containing the given address. */
const segment_t * segment_get_by_trampoline(const process_t * process, address_t address) {
    TREE_ITER(&process->segments, node) {
        const segment_t * segment = node->val;
        if (segment->trampolines) {
            /* Segment has trampolines -- get address range. */
            address_t low = segment->trampolines;
            address_t high = low + segment->trampolines_size;
            if ((low <= address) && (address < high))
                return segment;
        }
    }
    return NULL;
}

/**********************************************************************************************************************/

bool address_is_trampoline(const process_t * process, address_t address) {
    return segment_get_by_trampoline(process, address) != NULL;
}

bool address_is_injection(const process_t * process, address_t address) {
    return injection_get_by_address(process, address) != NULL;
}

bool address_is_artificial(const process_t * process, address_t address) {
    return address_is_injection(process, address) || address_is_trampoline(process, address);
}

/**********************************************************************************************************************/

static bool segment_set_executable(thread_t * thread, const segment_t * segment, bool executable) {
    if (!segment_is_executable(segment)) {
        /* Segment is not executable. */
        return true;
    }
    
    if (!segment->filename || segment->filename[0] != '/') {
        /* Segment is not mapped to a (regular) file. */
        return true;
    }
    
    int prot = (segment_is_readable(segment) ? PROT_READ : 0)
               | (segment_is_writeable(segment) ? PROT_WRITE : 0)
               | (executable ? PROT_EXEC : 0);
               
    return fncall_call_mprotect(thread, segment->start, segment->end - segment->start, prot);
}

bool segment_set_exacutable_all(process_t * process, bool executable) {

    TREE_ITER(&process->segments, node) {
        segment_t * segment = node->val;
        bool success = false;
        do {
            /* Pick a stopped thread. */
            thread_t * thread = thread_any_stopped(process);
            if (!thread) {
                /* There is no stopped thread, all threads are dead. */
                return true;
            }
            
            success = segment_set_executable(thread, segment, executable);
            
            if (!success) {
                if (!thread->state.dead) {
                    /* Thread didn't die. This is a serious failure */
                    thread_put(thread);
                    return false;
                } else {
                    /* Thread died.  Perhaps it was killed.  That's ok, pick another thread next time */
                }
            }
            
            thread_put(thread);
            
        } while (!success);
    }
    
    return true;
}

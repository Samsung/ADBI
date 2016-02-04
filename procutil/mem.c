#include "process/process.h"
#include "process/thread.h"

#include "process/segment.h"

#include "procutil/ptrace.h"
#include "procutil/procfs.h"

#include "mem.h"

#include <sys/param.h>

/* Mask a value into an word-aligned memory cell of the given process. The
 * function reads the original data at the given address and replaces the
 * bits corresponding to the lit bits in the masks with bits from the value
 * parameter. The result is written back to the process memory. Inferior memory
 * access is performed using ptrace.
 *
 * Returns true on success flag.
 *
 * Note: value of (~mask & value) must be 0.
 */
static bool mem_write_masked_word(thread_t * thread, address_t address, unsigned int value,
                                  unsigned int mask) {
    unsigned long t;
    
    assert(is_word_aligned(address));
    assert((~mask & value) == 0);
    
    if (!ptrace_mem_read(thread->pid, address, &t))
        return false;

    t &= ~((unsigned long) mask);
    t |= (unsigned long) value;
    
    return ptrace_mem_write(thread->pid, address, t);
}

/* Write a word or part of a word to the inferior process memory. Start and end
 * specify byte range to copy. Address must be word-aligned.
 *
 * Returns amount of bytes written.
 */
static size_t mem_write_shift_word(thread_t * thread, address_t address, char * data, unsigned int start,
                                   unsigned int end) {
                                   
    union arm_word {
        unsigned char byte[4];
        unsigned int  word;
    } value, mask;
    
    unsigned int offset;
    
    assert(is_word_aligned(address));
    assert(start < end);
    assert(start < 4);
    assert(end <= 4);
    
    /* TODO: Optimize full word writes. */
    
    for (offset = 0; offset < 4; ++offset) {
        if ((offset >= start) && (offset < end)) {
            mask.byte[offset] = 0xff;
            value.byte[offset] = data[offset];
        } else {
            mask.byte[offset] = 0x00;
            value.byte[offset] = 0x00;
        }
    }
    
    if (mem_write_masked_word(thread, address, value.word, mask.word))
        return (end - start);
    else
        return 0;
}

/* Copy bytes to inferior process. Source address range is (data + start) to
 * (data + end). Destination address range is (address + start) to
 * (address + end). The address value must be word aligned.
 *
 * Returns amount of bytes written.
 */
static size_t mem_write_shift(thread_t * thread, address_t address,
                              char * data, unsigned int start, unsigned int end) {
                              
    size_t written = 0;
    
    assert(start < 4);
    assert(start < end);
    assert(is_word_aligned(address));
    
    while (1) {
        size_t written_now = mem_write_shift_word(thread, address, data, start, end >= 4 ? 4 : end);
        written += written_now;
        
        if (written_now == 0)
            goto out;
            
        if (end <= 4)
            goto out;
            
        data += 4;
        address += 4;
        start = 0;
        end -= 4;
    }
    
out:
    return written;
}

/* Verify that size bytes of memory at the given address in the given process match the data buffer. Return true on
 * match, false on error or missmatch. */
static bool mem_verify(thread_t * thread, address_t address, size_t size, const void * data) {

    char contents[32];

    while (size) {
        size_t s = MIN(32, size);
        if (mem_read(thread, address, s, contents) != s)
            return false;

        if (memcmp(contents, data, s))
            return false;

        size -= s;
        address += s;
        data += s;
    }
    
    return true;
}

/* High level inferior memory access function. Copies size bytes starting at
 * data to the given process memory space. The function has no restrictions on
 * address or size alignment.
 *
 * Returns amount of bytes copied.
 */
size_t mem_write(thread_t * thread, address_t address, size_t size, void * data) {

    size_t written = 0;
    
    assert(thread);
    
    if (unlikely(thread->state.dead))
        return 0;
        
    /* The other functions assume that at least 1 byte is written. */
    
    if (likely(size)) {
    
        /* Aligned start address. */
        address_t first_word_address = address & ~0x03;
        
        /* End address. */
        address_t end = address + size;
        
        /* Start and end offsets from aligned start. */
        address_t start_offset  = address - first_word_address;
        address_t end_offset    = end - first_word_address;
        
        /* Convert to char ptr for pointer arithmetic. */
        char * data_char = (char *) data;
        /* Shift the source pointer as well. */
        data_char -= start_offset;
        
        written = mem_write_shift(thread, first_word_address, data_char, start_offset, end_offset);
        
        /* Verify written data. */
        assert(mem_verify(thread, address, written, data) || thread->state.dead);
    }
    
    if (written < size)
        thread_check_alive(thread);
        
    return written;
}

/* High level inferior memory access function. Copies size bytes starting at
 * address in the given process memory space to data. The function has no
 * restrictions on address or size alignment.
 *
 * Returns amount of bytes copied.
 */
size_t mem_read(thread_t * thread, address_t address, size_t count, void * data) {
    assert(thread);
    
    if (unlikely(thread->state.dead))
        return 0;
        
    /* We don't need to worry about alignment when reading from procfs. */
    size_t read = procfs_mem_read(thread, address, count, data);
    
    if (read < count)
        thread_check_alive(thread);
        
    return read;
}

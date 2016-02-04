#ifndef MMAP_H_
#define MMAP_H_

#include "common.h"
#include "errno.h"

#include "syscall_template.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_SEM 0x8
#define PROT_NONE 0x0
#define PROT_GROWSDOWN 0x01000000
#define PROT_GROWSUP 0x02000000

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_TYPE 0x0f
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20

#define MS_ASYNC 1
#define MS_INVALIDATE 2
#define MS_SYNC 4

#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4

#define MADV_REMOVE 9
#define MADV_DONTFORK 10
#define MADV_DOFORK 11

#define MAP_ANON MAP_ANONYMOUS
#define MAP_FILE 0

#define MREMAP_MAYMOVE  1
#define MREMAP_FIXED    2

/* Value returned when mmap fails. */
#define MAP_FAILED ((void *) -1)

/* Wrapper for mmap2 system. It does NOT set any errno, it is a RAW wrapper and
 * needs to be called very carefully. */
#ifdef __aarch64__
SYSCALL_6_ARGS(get_nr(__NR_mmap),
        void *, mmap, void * addr, size_t length, int prot, int flags, int fd, size_t offset);

SYSCALL_2_ARGS(get_nr(__NR_munmap),
        int,  munmap, void * address, size_t length);

SYSCALL_4_ARGS(get_nr(__NR_mremap),
        void *, mremap, void * old_address, size_t old_size, size_t new_size, unsigned long flags);

SYSCALL_3_ARGS(get_nr(__NR_mprotect),
        int, mprotect, void * address, size_t size, int prot);
#else
SYSCALL_6_ARGS(get_nr(192),
        void *, mmap, void * addr, size_t length, int prot, int flags, int fd, size_t offset);

SYSCALL_2_ARGS(get_nr(91),
        int,  munmap, void * address, size_t length);

SYSCALL_4_ARGS(get_nr(163),
        void *, mremap, void * old_address, size_t old_size, size_t new_size, unsigned long flags);

SYSCALL_3_ARGS(get_nr(125),
        int, mprotect, void * address, size_t size, int prot);
#endif

/*#define  MMAP2_SHIFT  12
static void * mmap(void * addr, size_t size, int prot, int flags, int fd, long offset) {

    void * result;
    
    if (offset & ((1UL << MMAP2_SHIFT) - 1)) {
        return (void *) - EINVAL;
    } else {
        result = __mmap2(addr, size, prot, flags, fd, (size_t) offset >> MMAP2_SHIFT);
        
        #if 0
        // Convert the result to a signed int.
        int i = (int) result;
        
        //* Normally, a negative syscall result means an error. However, in case
        //* of mmap, some "negative" results are correct. To make sure that an
        //* error has actually occurred, we check if the result is in the range
        //* of valid errno values.
        if ((i < 0) && (i > -256)) {
            // There was an error.
            result = MAP_FAILED;
        }
        #endif
    }
    
    return result;
    
}
#undef MMAP2_SHIFT*/

#endif

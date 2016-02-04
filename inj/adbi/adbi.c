#include "mmap.h"
#include "personality.h"
#include "mutex.h"
#include "varargs.h"

GLOBAL void * adbi_mmap(void * addr, size_t size, int prot, int flags, int fd, long offset) {
    return mmap(addr,                               /* address suggestion       */
                size,                               /* requested size           */
                prot,                               /* permissions              */
                flags,                              /* file mapping             */
                fd, offset                          /* fd and offset            */
               );
}

GLOBAL void * adbi_alloc(unsigned int size) {
    return mmap(NULL,                               /* no address suggestion    */
                size,                               /* requested size           */
                PROT_READ | PROT_WRITE | PROT_EXEC, /* all permissions          */
                MAP_PRIVATE | MAP_ANONYMOUS,        /* private mapping, no file */
                -1, 0                               /* fd and offset (ignored)  */
               );
}

GLOBAL int adbi_free(void * address, unsigned int size) {
    return munmap(address, size);
}

/* Get the error number of a system call. Return 0 if there was no error. */
LOCAL unsigned int get_errno(const void * r0_ptr) {
    /* System calls always return a return code. If it's positive or zero, the
     * call succeeded. If it's negative, there was an error. In this case the
     * value is set to -errno. However, some system calls, e.g. mmap, return
     * negative numbers in case of a success. For this reason we need to check
     * if the return value is negative and in the valid errno range. */
    signed int * val = (signed int *) r0_ptr;
    
    if ((*val < 0) && (*val > -256)) {
        return (unsigned int) - (*val);
    } else {
        return 0;
    }
}

/* Simple memcpy. This can be optimized, especially because we know that size is aligned to page size. */
LOCAL void adbi_memcpy(unsigned int * dst, unsigned int * src, unsigned int size) {
    size /= 4;
    while (size--)
        *dst++ = *src++;
}

GLOBAL void * adbi_realloc(void * old_address, unsigned int old_size, unsigned int new_size) {

    /* If necessary, we allow to move the memory to a different address. Moving
     * should never happen when shrinking, but may occur when there is not
     * enough continuous memory following the current block during extending.
     */
    void * ret = mremap(old_address, old_size, new_size, MREMAP_MAYMOVE);
    
    if (get_errno(&ret) != ENOMEM) {
        /* there was no error, or there was an unrecoverable error */
        goto out;
    }
    
    /* mremap failed, let's try to do it manually */
    ret = adbi_alloc(new_size);
    
    if (get_errno(&ret) != 0) {
        /* error, can't mmap */
        goto out;
    }
    
    /* mremap failed, but mmap didn't, copy memory contents */
    adbi_memcpy(ret, old_address, old_size < new_size ? old_size : new_size);
    
    /* We've got the memory copied, so now we can free the old pointer. */
    adbi_free(old_address, old_size);   /* XXX: This call can fail... */
    
out:
    return ret;
}

/**********************************************************************************************************************/

GLOBAL int adbi_mprotect(void * addr, size_t len, int prot) {

    bool hack = false;
    int ret, persona;
    
    if ((prot & PROT_READ) && !(prot & PROT_EXEC)) {
        /* The new protection allows reading, but it doesn't allow executing the memory.  The mprotect behavior in this
         * case depends on the current process personality.  If the personality has the READ_IMPLIES_EXEC bit set,
         * mprotect will allow execution of the segment automatically.  We need the protection to be exectly as
         * specified in prot, so we check the current personality and, if READ_IMPLIES_EXEC is set, we disable it
         * temporarily.  Note, that changing the setting only affect future calls to mprotect (and mmap), but it never
         * changes the protection flags, which were already set before the change occurred. */
        
        /* First read the current personality. */
        persona = personality(0xffffffff);
        ret = get_errno(&persona);
        if (ret) {
            /* Failed to get the current personality. */
            return ret;
        }
        
        /* We only need to use the hack if the personality has the READ_IMPLIES_EXEC bit set. */
        hack = persona & READ_IMPLIES_EXEC;
        
        if (hack) {
            /* Hack is requited. */
            ret = personality(persona & ~READ_IMPLIES_EXEC);
            ret = get_errno(&ret);
            if (ret) {
                /* Failed to change personality. */
                return ret;
            }
        }
        
    }
    
    /* Change protection flags. */
    ret = mprotect(addr, len, prot);
    
    if (hack) {
        /* We changed the personality before using mprotect.  It's time to revert the original value.  Note that we
         * don't care if the personality was actually changed.  We've already done our job, the memory protection was
         * changed, not reverting the READ_IMPLIES_EXEC bit shouldn't have impact on the process.   */
        personality(persona);
    }
    
    return ret;
}

/**********************************************************************************************************************/

#include "print.c"

/**********************************************************************************************************************/

INIT() {
    /* Initialize communication. */
    int ret = adbi_write_init();
    if (ret == 0) {
        /* The file was opened successfully.  Be polite and say hello. */
        adbi_printf("Hello ADBI.\n");
    }
    return ret;
}

EXIT() {
    return adbi_write_exit();
}

ADBI(adbi_mmap);
ADBI(adbi_alloc);
ADBI(adbi_free);
ADBI(adbi_realloc);
ADBI(adbi_mprotect);


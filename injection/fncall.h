#ifndef FNCALL_H
#define FNCALL_H

#include "arch.h"
#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"

typedef struct call_context_t call_context_t;

bool fncall_allocate(thread_t * thread, size_t size, address_t * address);
bool fncall_mmap(thread_t * thread, address_t * res, address_t addr, size_t size, int prot, int flags, int fd, long offset);
bool fncall_realloc(thread_t * thread, address_t * address, size_t old_size, size_t new_size);
bool fncall_free(thread_t * thread, address_t address, size_t size);

int fncall_get_errno(regval_t r0_val);
bool fncall_alloc_load_call(thread_t * thread, unsigned char * function_text,
                            size_t size, address_t entry, call_context_t * context);
size_t fncall_align_to_page(size_t size);

bool fncall_call_adbi(thread_t * thread, address_t address, int arg1, int arg2, int arg3, int arg4,
                      int * ret);
bool fncall_call_mprotect(thread_t * thread, address_t address, size_t size, int prot);

bool fncall_runtil(thread_t * thread, address_t stopat);
bool fncall_runtil_seg(thread_t * thread, segment_t * segment);

#endif

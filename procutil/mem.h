#ifndef MEM_H_
#define MEM_H_

#include <sys/types.h>  /* for size_t */

typedef struct thread_t thread_t;

size_t mem_write(thread_t * thread, address_t address, size_t size, void * data);
size_t mem_read(thread_t * thread, address_t address, size_t count, void * data);

#endif

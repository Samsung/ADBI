#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "common.h"

/* If we fail to allocate memory, just abort. Out of memory errors are so rare
 * today, that this shouldn't be a problem. Still, if it occurs, we should at
 * least tell the user what happened. */
void * adbi_malloc(size_t size) {
    void * result = malloc(size);
    if (unlikely(!result)) {
        fprintf(stderr, "Fatal: Failed to allocate %zu bytes of memory.\n", size);
        abort();
    }
    return result;
}

void * adbi_realloc(void * ptr, size_t size) {
    void * result = realloc(ptr, size);
    if (unlikely(!result)) {
        fprintf(stderr, "Fatal: Failed to allocate %zu bytes of memory.\n", size);
        abort();
    }
    return result;
}

void adbi_bug_(const char * file, int line, const char * function,
               const char * fmt, ...) {
               
    va_list args;
    
    fprintf(stderr,
            "Internal error in function %s at %s:%i:\n\t",
            function, file, line);
            
            
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fflush(stderr);
    
    abort();
}

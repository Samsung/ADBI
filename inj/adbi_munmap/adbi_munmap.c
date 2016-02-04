#include "common.h"
#include "syscall_template.h"

SYSCALL_2_ARGS(get_nr(__NR_munmap), int,  munmap, void * address, size_t length);

GLOBAL int adbi_munmap(void * address, unsigned int size) {
    return munmap(address, size);
}

INIT() {
    return 0;
}

ADBI(adbi_munmap);

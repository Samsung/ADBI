#include "common.h"
#include "syscall_template.h"

#ifdef __aarch64__
SYSCALL_6_ARGS(get_nr(__NR_mmap),
        void *, mmap, void * addr, size_t length, int prot, int flags, int fd, size_t offset);
#else
SYSCALL_6_ARGS(get_nr(192),
        void *, mmap, void * addr, size_t length, int prot, int flags, int fd, size_t offset);
#endif

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4


#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

GLOBAL
void * adbi_mmap(unsigned int size) {
    return mmap(NULL,                               /* no address suggestion    */
                size,                               /* requested size           */
                PROT_READ | PROT_WRITE | PROT_EXEC, /* all permissions          */
                MAP_PRIVATE | MAP_ANONYMOUS,        /* private mapping, no file */
                -1, 0                               /* fd and offset (ignored)  */
               );
}

INIT() {
    return 0;
}

ADBI(adbi_mmap);

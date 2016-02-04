#ifndef ELF_H_
#define ELF_H_

struct thread_t;

bool elf_is_elf64(FILE * file);
void * elf_get_local_linker_breakpoint_address();
void * elf_get_remote_linker_breakpoint_address(struct thread_t * thread);

#endif

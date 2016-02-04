#ifndef PATCH_H_
#define PATCH_H_

typedef struct thread_t thread_t;

bool patch_insn(thread_t * thread, address_t address, insn_kind_t kind, insn_t insn);
bool patch_read_insn(thread_t * thread, address_t address, insn_t * insn, insn_kind_t kind);
bool patch_read_insn_detect_kind(thread_t * thread, address_t address, insn_t * insn, insn_kind_t * kind);
void patch_breakpoint(thread_t * thread, address_t address, insn_kind_t kind);
void patch_relative_jump(thread_t * thread, address_t insn_address, address_t jump_address, insn_kind_t kind);

#endif

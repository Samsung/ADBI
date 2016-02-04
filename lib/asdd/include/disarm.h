#ifndef _DISARM_H
#define _DISARM_H

#include <stdbool.h>
#include <stdint.h>

/* The following functions disassemble a single instruction and return a string containing a human readable string.
 * The returned pointer points to an internal thread local buffer, which does not need to be freed.  Note that the
 * functions always return a pointer to the same buffer, only the contents of the buffer change. */
const char * disarm(uint32_t insn);
const char * disthumb(uint16_t insn);
const char * disthumb2(uint32_t insn);

/* The following functions are extended forms of the disassembling functions above.  They accept an additional pc
 * parameter, which specifies the address of the given instruction.  This value is used when evaluating branch
 * destinations and addresses. */
const char * disarma(uint32_t insn, uint32_t pc);
const char * disthumba(uint16_t insn, uint32_t pc);
const char * disthumb2a(uint32_t insn, uint32_t pc);

/* Return true if the given thumb opcode represents the first half of a 32-bit instruction (and should be disassembled)
 * using disthumb2 instead of disthumb. */
static inline bool is_thumb2(uint16_t insn) { return insn >= 0xe800; }

/* Merge two 16-bit thumb instructions to form a 32-bit thumb instruction. */
static inline uint32_t thumbcat(uint16_t first, uint16_t second) {
    return (((uint32_t) first) << 16) | ((uint32_t) second);
}

#endif /* _DISARM_H */
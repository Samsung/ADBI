#include "disarm.h"

/* Disassemble the given instruction of the given kind and return a string representing the instruction. The result
 * string may additionally include comments and more detailed information (like exact branch destination addresses).
 * Warning: the returned string is a thread-local variable. */
const char * arm_disassemble_extended(insn_t insn, insn_kind_t kind, address_t pc) {

    switch (kind) {
        case INSN_KIND_THUMB:
            return disthumba(insn, pc);
        case INSN_KIND_THUMB2:
            return disthumb2a(insn, pc);
        case INSN_KIND_ARM:
            return disarma(insn, pc);
        default:
            adbi_bug_unrechable();
            return "<undefined>";
    }
}

/* Disassemble the given instruction of the given kind and return a string representing the instruction.
 * Warning: the returned string is a thread-local variable. */
const char * arm_disassemble(insn_t insn, insn_kind_t kind) {
    char * disassembly = (char *) arm_disassemble_extended(insn, kind, 0);
    char * d;
    
    for (d = disassembly; *d; ++d) {
        if (*d == '\t') {
            /* replace tabs by spaces */
            *d = ' ';
        }
        
        if (*d == ';') {
            /* discard comments */
            *d = '\0';
            break;
        }
    }
    
    /* trim spaces on the right */
    for (--d; (d > disassembly) && *d == ' '; --d) *d = '\0';
    
    return disassembly;
}

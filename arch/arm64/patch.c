#include "process/process.h"
#include "procutil/mem.h"

#include "tracepoint/patch.h"
#include "process/thread.h"

/* Check if the given instruction is a correct instruction of the given kind. */
static int patch_check_insn(insn_kind_t kind, insn_t insn) {

    switch (kind) {
        case INSN_KIND_T32_16:
            /* A Thumb instruction can be only 16 bits long. */
            return (insn & ~0xffff) == 0;
        case INSN_KIND_T32_32:
            /* The first halfword must be correct. */
            return is_t32_32_halfword(t32_first_halfowrd(insn));
        case INSN_KIND_A32:
        case INSN_KIND_A64:
            /* No special requirements for ARM instruction. */
            return 1;
        default:
            adbi_bug_unrechable();
            return 0xbaadbaad;
    }
    
}

/* Patch an instruction at the given address in the given process. The
 * instruction is overwritten with the given instruction. The function does not
 * fail, but it requires:
 *      * all threads of the process to be stopped for tracing;
 *      * valid instruction alignment;
 *      * valid instructions;
 *      * valid address of the instruction (it must be accessible).
 */
bool patch_insn(thread_t * thread, address_t address, insn_kind_t kind, insn_t insn) {

    assert(arch_check_align(address, kind));
    assert(patch_check_insn(kind, insn));
    
    //debug("Patching instruction in process %s at %p with: %x\t%s", str_process(thread->process),
    //        (void *) address, insn, arm64_disassemble_extended(insn, kind, address));

    if (kind == INSN_KIND_T32_16) {
        uint16_t insn16 = (uint16_t) insn;
        return (mem_write(thread, address, 2, &insn16) == 2);
    } else {
        if (kind == INSN_KIND_T32_32)
            insn = t32_swap_halfwords(insn);
        return (mem_write(thread, address, 4, &insn) == 4);
    }
    
}

void patch_breakpoint(thread_t * thread, address_t address, insn_kind_t kind) {
    patch_insn(thread, address, kind, get_breakpoint_insn(kind));
}

void patch_relative_jump(thread_t * thread, address_t insn_address, address_t dest_address, insn_kind_t kind) {
    patch_insn(thread, insn_address, kind, arm64_get_relative_jump_insn(kind, insn_address, dest_address));
}

/* Read an instruction at the given address in the given process. The given thread must be stopped, the address must
 * be aligned properly.
 */
bool patch_read_insn(thread_t * thread, address_t address, insn_t * insn, insn_kind_t kind) {

    assert(arch_check_align(address, kind));
    
    if (kind == INSN_KIND_T32_16) {
        uint16_t insn16;
        if (mem_read(thread, address, 2, &insn16) != 2)
            return false;
        *insn = (insn_t) insn16;
    } else {
        if (mem_read(thread, address, 4, insn) != 4)
            return false;
        if (kind == INSN_KIND_T32_32)
            *insn = t32_swap_halfwords(*insn);
    }
    
    //debug("Reading instruction in process %s at %p: %x\t%s", str_process(thread->process),
    //        (void *) address, *insn, arm64_disassemble_extended(*insn, kind, address));

    return true;
}

/* Read an instruction at the given address in the given process. If the
 * instruction kind is INSN_KIND_THUMB or INSN_KIND_THUMB2, the function
 * will assign the correct kind value. Requirements:
 *      * all threads of the process to be stopped for tracing;
 *      * valid instruction alignment;
 *      * correct instruction kind;
 *      * valid address of the instruction (it must be accessible).
 */
bool patch_read_insn_detect_kind(thread_t * thread, address_t address, insn_t * insn, insn_kind_t * kind) {

    if (*kind == INSN_KIND_A64 || *kind == INSN_KIND_A32) {
        /* ARM instructions are always 32 bit long. */
        return patch_read_insn(thread, address, insn, *kind);
    } else {
        /* Thumb instructions can be 32 or 16 bit long. Try the first 16 bits first. */
        if (!patch_read_insn(thread, address, insn, INSN_KIND_T32_16))
            return false;
            
        if (is_t32_32_halfword(*insn)) {
            *kind = INSN_KIND_T32_32;
            return patch_read_insn(thread, address, insn, INSN_KIND_T32_32);
        } else {
            *kind = INSN_KIND_T32_16;
            return true;
        }
    }
    
}

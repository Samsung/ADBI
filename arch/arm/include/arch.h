#ifndef INSN_H
#define INSN_H

#include <stdint.h>
#include <sys/ptrace.h>

typedef struct pt_regs pt_regs;

/* Type representing memory address. */
//typedef uintptr_t   address_t;
typedef unsigned long   address_t;
typedef off_t    offset_t;

/* Instruction types. */
typedef enum insn_kind_t {
    INSN_KIND_THUMB = 2,
    INSN_KIND_THUMB2 = 3,
    INSN_KIND_ARM = 4,
} insn_kind_t;

/* Instruction code type. */
typedef uint32_t insn_t;

typedef uint32_t regval_t;
typedef int32_t  sregval_t;
typedef uint32_t stackval_t;

/* Function call context */
typedef struct call_context_t {
    regval_t registers[4];
    unsigned int stack_elements;
    stackval_t * stack;
} call_context_t;

#define call_context_empty { {0,0,0,0}, 0, NULL }

#define BREAKPOINT_INSN_ARM     0xe1200070    /* bkpt A1 */
#define BREAKPOINT_INSN_THUMB   0xde00
#define BREAKPOINT_INSN_THUMB2  0xf7f0a000

#define BRANCH_INSN_ARM           0xea000000
#define BRANCH_INSN_THUMB         0xe000
#define BRANCH_INSN_THUMB2        0xf0009000

#define NOP_INSN_ARM            0xe320f000
#define NOP_INSN_THUMB          0x46c0
#define NOP_INSN_THUMB2         0xf3af8000

#define is_thumb_mode(regs) ((regs)->ARM_cpsr & PSR_T_BIT)
#define is_arm_mode(regs)   (!is_thumb_mode(regs))

#define arm_is_mode32(regs) ((regs)->ARM_cpsr & MODE32_BIT)
#define arch_is_mode32 arm_is_mode32

#define arm_get_execution_mode_insn_kind(regs) (((regs)->ARM_cpsr & PSR_T_BIT) ? INSN_KIND_THUMB : INSN_KIND_ARM)
#define arch_get_execution_mode_insn_kind arm_get_execution_mode_insn_kind

#define arm_pt_regs_pc(regs) ((regs)->ARM_pc)
#define arm_pt_regs_sp(regs) ((regs)->ARM_sp)
#define arm_pt_regs_lr(regs) ((regs)->ARM_lr)

#define arch_pt_regs_pc arm_pt_regs_pc
#define arch_pt_regs_sp arm_pt_regs_sp
#define arch_pt_regs_lr arm_pt_regs_lr

/* Check if the given 16 bit instruction is the first halfword of a 32-bit
 * Thumb instruction. */
static inline int is_thumb2_halfword(insn_t insn) {
    /* Make sure the instruction is 16 bit long. */
    assert(insn <= 0xffff);
    insn &= 0xf800;
    return (insn >= 0xe800);
}

static inline insn_t thumb2_first_halfowrd(insn_t insn) {
    return insn >> 16;
}

static inline insn_t thumb2_last_halfowrd(insn_t insn) {
    return insn & 0xffff;
}

static inline insn_t thumb2_swap_halfwords(insn_t insn) {
    return ((insn & 0xffff) << 16) |
           (insn >> 16);
}

static inline insn_t get_breakpoint_insn(insn_kind_t kind) {
    switch (kind) {
        case INSN_KIND_ARM:
            return BREAKPOINT_INSN_ARM;
        case INSN_KIND_THUMB:
            return BREAKPOINT_INSN_THUMB;
        case INSN_KIND_THUMB2:
            return BREAKPOINT_INSN_THUMB2;
        default:
            adbi_bug_unrechable();
            return 0;
    }
}

/*
 * A2.3 PC, the program counter
 * When executing an ARM instruction, PC reads as the address of the current instruction plus 8.
 * When executing a Thumb instruction, PC reads as the address of the current instruction plus 4.
 */
static inline address_t arm_pc_to_address(insn_kind_t kind, address_t addr) {
    if (kind == INSN_KIND_ARM)
        return addr - 8;
    else
        return addr - 4;
}

static inline address_t arm_address_to_pc(insn_kind_t kind, address_t addr) {
    if (kind == INSN_KIND_ARM)
        return addr + 8;
    else
        return addr + 4;
}

#define BRANCH_T1_JUMP_MIN       -256
#define BRANCH_T1_JUMP_MAX        254
#define BRANCH_T2_JUMP_MIN      -2048
#define BRANCH_T2_JUMP_MAX       2046
#define BRANCH_T3_JUMP_MIN   -1048576
#define BRANCH_T3_JUMP_MAX    1048574
#define BRANCH_T4_JUMP_MIN  -16777216
#define BRANCH_T4_JUMP_MAX   16777214
#define BRANCH_A1_JUMP_MIN  -33554432
#define BRANCH_A1_JUMP_MAX   33554428

static inline bool arm_check_relative_jump_kind(insn_kind_t kind, address_t addr, address_t dest) {
    addr = arm_address_to_pc(kind, addr);
    address_t distance = addr < dest ? (dest - addr) : (addr - dest);
    if (distance > 33554432)
        return false;
    offset_t jump = dest - addr;
    switch (kind) {
        case INSN_KIND_ARM:
            return (BRANCH_A1_JUMP_MIN <= jump) && (jump <= BRANCH_A1_JUMP_MAX);// && ((jump & 0x03) == 0);
        case INSN_KIND_THUMB:
            return (BRANCH_T2_JUMP_MIN <= jump) && (jump <= BRANCH_T2_JUMP_MAX);// && ((jump & 0x01) == 0);
        case INSN_KIND_THUMB2:
            return (BRANCH_T4_JUMP_MIN <= jump) && (jump <= BRANCH_T4_JUMP_MAX);// && ((jump & 0x01) == 0);
        default:
            adbi_bug_unrechable();
            return 0;
    }
}

#define arch_check_relative_jump_kind arm_check_relative_jump_kind
#define arch_check_relative_jump(addr, dest) arm_check_relative_jump_kind(INSN_KIND_ARM, (addr), (dest))

static inline insn_t thumb2_branch(offset_t jump) {
    insn_t S     = !!(jump & 0x01000000);
    insn_t I1    = !!(jump & 0x00800000);
    insn_t I2    = !!(jump & 0x00400000);
    insn_t J1    = !(I1 ^ S);
    insn_t J2    = !(I2 ^ S);
    insn_t imm10 = (jump & 0x003ff000) << 4;
    insn_t imm11 = (jump & 0xffe) >> 1;
    return BRANCH_INSN_THUMB2 | (S << 26) | imm10 | (J1 << 13) | (J2 << 11) | imm11;
}

static inline insn_t arm_get_relative_jump_insn(insn_kind_t kind, address_t addr, address_t dest) {
    addr = arm_address_to_pc(kind, addr);
    assert((addr < dest ? (dest - addr) : (addr - dest)) <= 33554432);
    offset_t jump = dest - addr;
    switch (kind) {
        case INSN_KIND_ARM:
            return BRANCH_INSN_ARM | ((jump >> 2) & 0xffffff);
        case INSN_KIND_THUMB:
            return BRANCH_INSN_THUMB | ((jump >> 1) & 0x7ff);
        case INSN_KIND_THUMB2:
            return thumb2_branch(jump);
        default:
            adbi_bug_unrechable();
            return 0;
    }
}

#define arch_get_relative_jump_insn arm_get_relative_jump_insn

static inline const char * insn_kind_str(insn_kind_t kind) {
    switch (kind) {
        case INSN_KIND_ARM:
            return "ARM";
        case INSN_KIND_THUMB:
            return "Thumb";
        case INSN_KIND_THUMB2:
            return "Thumb2";
        default:
            adbi_bug_unrechable();
            return "<ERROR>";
    }
}

/* Check the given instruction of the given kind. If the instruction is invalid, try to fix it and update *insn and/or
 * *kind.  Return non-zero iff the instruction was ok or is fixed now. */
static inline int arm_check_insn(insn_t * insn, insn_kind_t * kind) {
    switch (*kind) {
        case INSN_KIND_ARM:
            return 1;
        case INSN_KIND_THUMB:
        case INSN_KIND_THUMB2:
            if (*insn < 0xe800) {
                /* 16-bit Thumb instruction */
                *kind = INSN_KIND_THUMB;
                return 1;
            } else if (is_thumb2_halfword(thumb2_first_halfowrd(*insn))) {
                /* 32-bit Thumb instruction */
                *kind = INSN_KIND_THUMB2;
                return 1;
            } else {
                /* Invalid instruction */
                return 0;
            }
        default:
            adbi_bug_unrechable();
            return 0;
    }
}
#define arch_check_insn arm_check_insn

/* Check if the given address is correctly aligned for the given instruction kind. */
static inline int arm_check_align(address_t address, insn_kind_t kind) {
    address_t align_bits;
    switch (kind) {
        case INSN_KIND_ARM:
            align_bits = 0x03;
            break;
        case INSN_KIND_THUMB2:
        case INSN_KIND_THUMB:
            align_bits = 0x01;
            break;
        default:
            adbi_bug_unrechable();
    }
    
    return (address & align_bits) == 0;
}
#define arch_check_align arm_check_align

static inline address_t arm_align_address_kind(address_t address, insn_kind_t kind) {
    address_t align_bits;
    switch (kind) {
        case INSN_KIND_ARM:
            align_bits = 0x03;
            break;
        case INSN_KIND_THUMB2:
        case INSN_KIND_THUMB:
            align_bits = 0x01;
            break;
        default:
            adbi_bug_unrechable();
    }

    return (address & (~align_bits));
}
#define arch_align_address_kind arm_align_address_kind

#define arm_align_address(address) (address & (~0x1))
#define arch_align_address arm_align_address

#define arm_detect_kind_from_unaligned_address(mode32, address) ((address & 0x1) ? INSN_KIND_THUMB : INSN_KIND_ARM)
#define arch_detect_kind_from_unaligned_address arm_detect_kind_from_unaligned_address

const char * arm_disassemble_extended(insn_t insn, insn_kind_t kind, address_t pc);

const char * arm_disassemble(insn_t insn, insn_kind_t kind);
#define arch_disassemble arm_disassemble

struct thread_t;
struct process_t;
struct tracepoint_t;

void arm_disassemble_handler(struct thread_t * thread, struct tracepoint_t * tracepoint, void * code);
#define arch_disassemble_handler arm_disassemble_handler

void dump_thread(struct thread_t * thread);

static inline address_t arch_get_pc(const pt_regs * regs) {
    return regs->ARM_pc & 0xfffffffe;
}

#ifndef instruction_pointer
#define PCMASK 0
#define pc_pointer(v) ((v) & ~PCMASK)
#define instruction_pointer(regs) (pc_pointer((regs)->ARM_pc))
#define profile_pc(regs) instruction_pointer(regs)
#endif

#endif

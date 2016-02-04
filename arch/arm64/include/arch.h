#ifndef ARCH_ARM64_INCLUDE_ARCH_H_
#define ARCH_ARM64_INCLUDE_ARCH_H_

#include <stdint.h>
#include <sys/ptrace.h>

typedef struct user_pt_regs pt_regs;

/* Type representing memory address. */
typedef uintptr_t   address_t;
typedef off_t    offset_t;

/* Instruction types. */
typedef enum insn_kind_t {
    INSN_KIND_T32_16 = 2,
    INSN_KIND_T32_32 = 3,
    INSN_KIND_A32 = 4,
    INSN_KIND_A64 = 5,
} insn_kind_t;

/* Instruction code type. */
typedef uint32_t insn_t;

typedef uint64_t regval_t;
typedef int64_t  sregval_t;
typedef uint64_t stackval_t;
typedef uint32_t stackval32_t;

/* Function call context */
typedef struct call_context_t {
    regval_t registers[8];
    unsigned int stack_elements;
    union {
        stackval_t   * stack;
        stackval32_t * stack32;
    };
    bool mode32;
} call_context_t;

#define call_context_empty { {0,0,0,0,0,0,0,0}, 0, NULL, false }

#define BREAKPOINT_INSN_T32_16  0xde00        /* udf #0 T1 */
#define BRANCH_INSN_T32_16      0xe000        /* b T2 */
#define NOP_INSN_T32_16         0x46c0        /* nop T1 */

#define BREAKPOINT_INSN_T32_32  0xf7f0a000    /* udf #0 T2 */
#define BRANCH_INSN_T32_32      0xf0009000    /* b T4 */
#define NOP_INSN_T32_32         0xf3af8000    /* nop T2 */

#define BREAKPOINT_INSN_A32     0xe1200070    /* bkpt A1 */
#define BRANCH_INSN_A32         0xea000000    /* b A1*/
#define NOP_INSN_A32            0xe320f000    /* nop A1*/

#define BREAKPOINT_INSN_A64     0xd4200000    /* brk, imm16 = 0 */
#define BRANCH_INSN_A64         0x14000000    /* b */
#define NOP_INSN_A64            0xd503201f    /* hint #0 */

#ifndef PSR_T_BIT
#define PSR_T_BIT     0x00000020
#endif

#define arm64_is_mode32(regs) ((regs)->pstate & PSR_MODE32_BIT)
#define arch_is_mode32 arm64_is_mode32

static inline insn_kind_t arm64_get_execution_mode_insn_kind(pt_regs * regs) {
    if (arm64_is_mode32(regs)) {
        if (regs->pstate & PSR_T_BIT)
            return INSN_KIND_T32_32;
        return INSN_KIND_A32;
    }
    return INSN_KIND_A64;
}

#define arch_get_execution_mode_insn_kind arm64_get_execution_mode_insn_kind

#define arm64_pt_regs_pc(regs) ((regs)->pc)
#define arm64_pt_regs_sp(regs) ((regs)->sp)
#define arm64_pt_regs_lr(ptregs) ((regs)->regs[30])

#define arch_pt_regs_pc arm64_pt_regs_pc
#define arch_pt_regs_sp arm64_pt_regs_sp
#define arch_pt_regs_lr arm64_pt_regs_lr

/* Check if the given 16 bit instruction is the first halfword of a 32-bit
 * Thumb instruction. */
static inline int is_t32_32_halfword(insn_t insn) {
    /* Make sure the instruction is 16 bit long. */
    assert(insn <= 0xffff);
    insn &= 0xf800;
    return (insn >= 0xe800);
}

static inline insn_t t32_first_halfowrd(insn_t insn) {
    return insn >> 16;
}

static inline insn_t t32_last_halfowrd(insn_t insn) {
    return insn & 0xffff;
}

static inline insn_t t32_swap_halfwords(insn_t insn) {
    return ((insn & 0xffff) << 16) |
           (insn >> 16);
}

static inline insn_t get_breakpoint_insn(insn_kind_t kind) {
    switch (kind) {
    case INSN_KIND_T32_16:
        return BREAKPOINT_INSN_T32_16;
    case INSN_KIND_T32_32:
        return BREAKPOINT_INSN_T32_32;
    case INSN_KIND_A32:
        return BREAKPOINT_INSN_A32;
    case INSN_KIND_A64:
        return BREAKPOINT_INSN_A64;
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
static inline address_t arm64_pc_to_address(insn_kind_t kind, address_t addr) {
    switch (kind) {
    case INSN_KIND_T32_16:
    case INSN_KIND_T32_32:
        return addr - 4;
    case INSN_KIND_A32:
        return addr - 8;
    case INSN_KIND_A64:
        return addr;
    default:
        adbi_bug_unrechable();
        return 0;
    }
}

static inline address_t arm64_address_to_pc(insn_kind_t kind, address_t addr) {
    switch (kind) {
    case INSN_KIND_T32_16:
    case INSN_KIND_T32_32:
        return addr + 4;
    case INSN_KIND_A32:
        return addr + 8;
    case INSN_KIND_A64:
        return addr;
    default:
        adbi_bug_unrechable();
        return 0;
    }
}

#define BRANCH_T32_T1_JUMP_MIN       -256 /*   256 imm8       0x80 << 1 */
#define BRANCH_T32_T1_JUMP_MAX        254 /*                  0x7f << 1 */
#define BRANCH_T32_T2_JUMP_MIN      -2048 /*   2KB imm11     0x400 << 1 */
#define BRANCH_T32_T2_JUMP_MAX       2046 /*                 0x3ff << 1 */
#define BRANCH_T32_T3_JUMP_MIN   -1048576 /*   1MB imm17   0x10000 << 1 */
#define BRANCH_T32_T3_JUMP_MAX    1048574 /*               0x0ffff << 1 */
#define BRANCH_T32_T4_JUMP_MIN  -16777216 /*  16MB imm21  0x100000 << 1 */
#define BRANCH_T32_T4_JUMP_MAX   16777214 /*              0x0fffff << 1 */
#define BRANCH_A32_A1_JUMP_MIN  -33554432 /*  32MB imm24  0x800000 << 2 */
#define BRANCH_A32_A1_JUMP_MAX   33554428 /*              0x7fffff << 2 */
#define BRANCH_A64_JUMP_MIN    -134217728 /* 128MB imm26 0x2000000 << 2 */
#define BRANCH_A64_JUMP_MAX     134217724 /*             0x1ffffff << 2 */

#define BRANCH_MAX_ABS          134217728

static inline bool arm64_check_relative_jump_kind(insn_kind_t kind, address_t addr, address_t dest) {
    addr = arm64_address_to_pc(kind, addr);
    address_t distance = addr < dest ? (dest - addr) : (addr - dest);
    if (distance > BRANCH_MAX_ABS)
        return false;
    offset_t jump = dest - addr;
    switch (kind) {
        case INSN_KIND_T32_16:
            return (BRANCH_T32_T2_JUMP_MIN <= jump) && (jump <= BRANCH_T32_T2_JUMP_MAX);
        case INSN_KIND_T32_32:
            return (BRANCH_T32_T4_JUMP_MIN <= jump) && (jump <= BRANCH_T32_T4_JUMP_MAX);
        case INSN_KIND_A32:
            return (BRANCH_A32_A1_JUMP_MIN <= jump) && (jump <= BRANCH_A32_A1_JUMP_MAX);
        case INSN_KIND_A64:
            return (BRANCH_A64_JUMP_MIN <= jump) && (jump <= BRANCH_A64_JUMP_MAX);
        default:
            adbi_bug_unrechable();
            return 0;
    }
}

#define arch_check_relative_jump_kind arm64_check_relative_jump_kind
#define arch_check_relative_jump(addr, dest) arm64_check_relative_jump_kind(INSN_KIND_A64, (addr), (dest))

static inline insn_t t32_16_branch(offset_t jump) {
    insn_t S     = !!(jump & 0x01000000);
    insn_t I1    = !!(jump & 0x00800000);
    insn_t I2    = !!(jump & 0x00400000);
    insn_t J1    = !(I1 ^ S);
    insn_t J2    = !(I2 ^ S);
    insn_t imm10 = (jump & 0x003ff000) << 4;
    insn_t imm11 = (jump & 0xffe) >> 1;
    return BRANCH_INSN_T32_16 | (S << 26) | imm10 | (J1 << 13) | (J2 << 11) | imm11;
}

static inline insn_t arm64_get_relative_jump_insn(insn_kind_t kind, address_t addr, address_t dest) {
    addr = arm64_address_to_pc(kind, addr);
    assert((addr < dest ? (dest - addr) : (addr - dest)) <= BRANCH_MAX_ABS);
    offset_t jump = dest - addr;
    switch (kind) {
        case INSN_KIND_T32_16:
            return t32_16_branch(jump);
        case INSN_KIND_T32_32:
            return BRANCH_INSN_T32_32 | ((jump >> 1) & 0x7ff);
        case INSN_KIND_A32:
            return BRANCH_INSN_A32 | ((jump >> 2) & 0xffffff);
        case INSN_KIND_A64:
            return BRANCH_INSN_A64 | ((jump >> 2) & 0x3ffffff);
        default:
            adbi_bug_unrechable();
            return 0;
    }
}

#define arch_get_relative_jump_insn arm64_get_relative_jump_insn

static inline const char * insn_kind_str(insn_kind_t kind) {
    switch (kind) {
        case INSN_KIND_T32_16:
            return "T32 16-bit";
        case INSN_KIND_T32_32:
            return "T32 32-bit";
        case INSN_KIND_A32:
            return "A32";
        case INSN_KIND_A64:
            return "A64";
        default:
            adbi_bug_unrechable();
            return "<ERROR>";
    }
}

/* Check the given instruction of the given kind. If the instruction is invalid, try to fix it and update *insn and/or
 * *kind.  Return non-zero if the instruction was ok or is fixed now. */
static inline int arm64_check_insn(insn_t * insn, insn_kind_t * kind) {
    switch (*kind) {
    case INSN_KIND_T32_16:
    case INSN_KIND_T32_32:
        if (*insn < 0xe800) {
        /* 16-bit Thumb instruction */
            *kind = INSN_KIND_T32_16;
            return 1;
        } else if (is_t32_32_halfword(t32_first_halfowrd(*insn))) {
            /* 32-bit Thumb instruction */
            *kind = INSN_KIND_T32_32;
            return 1;
        } else {
            /* Invalid instruction */
            return 0;
        }
    case INSN_KIND_A32:
    case INSN_KIND_A64:
        return 1;
    default:
        adbi_bug_unrechable();
        return 0;
}
}
#define arch_check_insn arm64_check_insn

static inline address_t get_align_bits(insn_kind_t kind) {
    switch (kind) {
    case INSN_KIND_T32_16:
    case INSN_KIND_T32_32:
        return 0x01;
    case INSN_KIND_A32:
    case INSN_KIND_A64:
        return 0x03;
    default:
        adbi_bug_unrechable();
        return 0;
    }
}

/* Check if the given address is correctly aligned for the given instruction kind. */
static inline int arm64_check_align(address_t address, insn_kind_t kind) {
    address_t align_bits = get_align_bits(kind);
    return (address & align_bits) == 0;
}
#define arch_check_align arm64_check_align

static inline address_t arm64_align_address(address_t address, insn_kind_t kind) {
    address_t align_bits = get_align_bits(kind);
    return (address & (~align_bits));
}
#define arch_align_address arm64_align_address

#define arm64_detect_kind_from_unaligned_address(mode32, address) (mode32 ? ((address & 0x1) ? INSN_KIND_T32_32 : INSN_KIND_A32) : INSN_KIND_A64)
#define arch_detect_kind_from_unaligned_address arm64_detect_kind_from_unaligned_address

const char * arm64_disassemble_extended(insn_t insn, insn_kind_t kind, address_t pc);

const char * arm64_disassemble(insn_t insn, insn_kind_t kind);
#define arch_disassemble arm64_disassemble

struct thread_t;
struct process_t;
struct tracepoint_t;

void arm64_disassemble_handler(struct thread_t * thread, struct tracepoint_t * tracepoint, void * code);
#define arch_disassemble_handler arm64_disassemble_handler

void dump_thread(struct thread_t * thread);

static inline address_t arch_get_pc(const pt_regs * regs) {
    return regs->pc;
}

#ifndef instruction_pointer
#define PCMASK 0
#define pc_pointer(v) ((v) & ~PCMASK)
#define instruction_pointer(regs) (pc_pointer((regs)->pc))
#define profile_pc(regs) instruction_pointer(regs)
#endif

#endif /* ARCH_ARM64_INCLUDE_ARCH_H_ */

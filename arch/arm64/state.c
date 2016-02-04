#include <asm/ptrace.h>

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"

#include "procutil/mem.h"
#include "tracepoint/patch.h"

#ifndef PSR_E_BIT
#define PSR_E_BIT (1 << 9)  // AArch64 PSR_D_BIT
#endif

#ifndef PSR_T_BIT
#define PSR_T_BIT (1 << 5)
#endif

static void state_processor(thread_t * thread, pt_regs * regs) {
    regval_t pstate = regs->pstate;
    bool mode32 = arch_is_mode32(regs);

    const char * endianess_str(regval_t cpsr) {
        return (cpsr & PSR_E_BIT) ? "big endian" : "little endian";
    }
    
    const char * exceptions_str(regval_t psr) {
        const char * enabled_str(regval_t val) {
            return val ? "enabled" : "disabled";
        }
        static __thread char buf[80];
        
        uint32_t aabt = ((psr & PSR_A_BIT) == 0);
        uint32_t fiq = ((psr & PSR_I_BIT) == 0);
        uint32_t irq = ((psr & PSR_F_BIT) == 0);
        
        if (mode32) {
            uint32_t all = fiq && irq && aabt;
        
            if (all)
                return "all enabled";

            snprintf(buf, 64, "async abort %s, IRQ %s, FIQ %s", enabled_str(aabt),
                    enabled_str(irq), enabled_str(fiq));

        } else {
            uint32_t dbg = ((psr & PSR_D_BIT) == 0);
            uint32_t all = fiq && irq && aabt && dbg;

            if (all)
                return "all enabled";
            
            snprintf(buf, 64, "async abort %s, IRQ %s, FIQ %s, debug %s", enabled_str(aabt),
                    enabled_str(irq), enabled_str(fiq), enabled_str(dbg));
        }

        return buf;
    }
    
    const char * it_str(regval_t cpsr) {
    
        static __thread char buf[64];
        
        uint32_t it = ((cpsr >> 10) & 0b111111) | ((cpsr >> 25) & 0b11);
        uint32_t base_cond = it >> 5;
        uint32_t it_40 = it & 0b11111;
        uint32_t conditional_count;
        
        if (!it)
            return "no IT block active";
            
        for (conditional_count = 0; it_40;
                ++conditional_count, it_40 = (it_40 << 1) & 0x11111)
            ;
            
        snprintf(buf, 64,
                 "condition %1x, conditional instructions remaining: %u ",
                 base_cond, conditional_count);
                 
        return buf;
    }
    
    const char * mode_str(regval_t psr) {
        if (mode32) {
            uint32_t mode = psr & 0x1f; // MODE_MASK
            switch (mode) {
            case 0x10: // USR_MODE
                return "usr";
            case 0x11: // FIQ_MODE
                return "fiq";
            case 0x12: // SVC_MODE
                return "svc";
            case 0x17: // ABT_MODE
                return "abt";
            case 0x1b: // UND_MODE
                return "und";
            case 0x1f: // SYSTEM_MODE
                return "sys";
            default:
                return "???";
            }
        } else {
            uint32_t mode = psr & PSR_MODE_MASK;
            switch (mode) {
            case PSR_MODE_EL3h:
                return "EL3h";
            case PSR_MODE_EL3t:
                return "EL3t";
            case PSR_MODE_EL2h:
                return "EL2h";
            case PSR_MODE_EL2t:
                return "EL2t";
            case PSR_MODE_EL1h:
                return "EL1h";
            case PSR_MODE_EL1t:
                return "EL1t";
            case PSR_MODE_EL0t:
                return "EL0t";
            default:
                return "????";
            }
        }
    }
    
    const char * cond_str(regval_t psr) {
        static __thread char buf[] = "     ";
        buf[0] = psr & PSR_N_BIT ? 'N' : ' ';
        buf[1] = psr & PSR_Z_BIT ? 'Z' : ' ';
        buf[2] = psr & PSR_C_BIT ? 'C' : ' ';
        buf[3] = psr & PSR_V_BIT ? 'V' : ' ';
        if (mode32)
            buf[4] = psr & PSR_Q_BIT ? 'Q' : ' ';
        return buf;
    }
    
    const char * state_str(regval_t psr) {
        if (psr & PSR_MODE32_BIT) {
            return (psr & PSR_T_BIT) ? "T32" : "A32";
        } else {
            return "A64";
        }
    }
    
    address_t pc = instruction_pointer(regs);
    verbose("Processor state:");
    verbose(" Program counter: %s", str_address(thread->process, pc));
    verbose(" State:           %s", state_str(pstate));
    verbose(" Mode:            %s", mode_str(pstate));
    verbose(" Exceptions:      %s", exceptions_str(pstate));
    verbose(" Condition flags: [%s]", cond_str(pstate));
    if (mode32) {
        verbose(" Endianess:       %s", endianess_str(pstate));
        verbose(" IT:              %s", it_str(pstate));
    }
    
    {
        insn_kind_t kind = (pstate & PSR_MODE32_BIT) ? (pstate & PSR_T_BIT ? INSN_KIND_T32_32 : INSN_KIND_A32) : INSN_KIND_A64;
        insn_t insn;
        if (arch_check_align(pc, kind) && patch_read_insn_detect_kind(thread, pc, &insn, &kind))
            if (kind == INSN_KIND_T32_16)
                verbose(" Instruction:         %04x\t%s", insn, arm64_disassemble(insn, kind));
            else if (kind == INSN_KIND_T32_32)
                verbose(" Instruction:         %04x %04x\t%s", t32_first_halfowrd(insn),
                        t32_last_halfowrd(insn), arm64_disassemble(insn, kind));
            else
                verbose(" Instruction:         %08x\t%s", insn, arm64_disassemble(insn, kind));
        else if (mem_read(thread, pc, 4, &insn) == 4)
            verbose(" Instruction:         %08x\t(invalid alignment)", insn);
        else
            verbose(" Instruction:         unknown\t(invalid alignment)");
    }
}

static void state_registers(thread_t * thread, pt_regs * regs) {

    process_t * process = thread->process;
    
    verbose("Register values:");
#define print_reg_n(n)                                          \
    verbose(" %7s  %s",                                       \
            "x"# n,                                           \
            str_address(process, regs->regs[n]))

    print_reg_n(0);
    print_reg_n(1);
    print_reg_n(2);
    print_reg_n(3);
    print_reg_n(4);
    print_reg_n(5);
    print_reg_n(6);
    print_reg_n(7);

    verbose(" xr   x8  %s", str_address(process, regs->regs[8]));

    print_reg_n(9);
    print_reg_n(10);
    print_reg_n(11);
    print_reg_n(12);
    print_reg_n(13);
    print_reg_n(14);
    print_reg_n(15);

    verbose(" ip0 x16  %s", str_address(process, regs->regs[16]));
    verbose(" ip1 x17  %s", str_address(process, regs->regs[17]));
    verbose(" pr  x18  %s", str_address(process, regs->regs[18]));

    print_reg_n(19);
    print_reg_n(20);
    print_reg_n(21);
    print_reg_n(22);
    print_reg_n(23);
    print_reg_n(24);
    print_reg_n(25);
    print_reg_n(26);
    print_reg_n(27);
    print_reg_n(28);
    
    verbose(" fp  x29  %s", str_address(process, regs->regs[29]));
    verbose(" lr  x30  %s", str_address(process, regs->regs[30]));

    verbose(" sp       %s", str_address(process, regs->sp));
    verbose(" pc       %s", str_address(process, regs->pc));
    verbose(" pstate   %llx", regs->pstate);
    
#undef print_reg_n
}

static void state_stack(thread_t * thread, pt_regs * regs, size_t count) {

    regval_t sp = regs->sp;
    size_t i;
    verbose("Stack:");
    if (thread->process->mode32) {
        for (i = 0; i < count; ++i) {
            uint32_t val;
            if (mem_read(thread, sp, 4, &val) == 8)
                verbose(" %2u  sp+%02x  0x%08lx  %s",
                        (unsigned int) i,
                        (unsigned int) i * 4,
                        sp,
                        str_address(thread->process, val));
            else
                verbose(" %2u  sp+%02x  0x%08lx  ???",
                        (unsigned int) i,
                        (unsigned int) i * 4,
                        sp);
            sp += 4;
        }
    } else {
        for (i = 0; i < count; ++i) {
            uint64_t val;
            if (mem_read(thread, sp, 8, &val) == 8)
                verbose(" %2u  sp+%02x  0x%016lx  %s",
                        (unsigned int) i,
                        (unsigned int) i * 8,
                        sp,
                        str_address(thread->process, val));
            else
                verbose(" %2u  sp+%02x  0x%016lx  ???",
                        (unsigned int) i,
                        (unsigned int) i * 8,
                        sp);
            sp += 8;
        }
    }
}

typedef struct frame_record_t {
    regval_t fp;
    regval_t lr;
} frame_record_t;

static void state_backtrace(thread_t * thread, pt_regs * regs) {
    frame_record_t fr = { regs->regs[29], regs->regs[30] };
    unsigned int level = 0;
    debug("Backtrace:");
    debug(" %02u  %s", level++, str_address(thread->process, regs->pc));
    do {
        debug(" %02u  %s", level++, str_address(thread->process, fr.lr - 4));
    } while (fr.fp && (mem_read(thread, fr.fp, sizeof(frame_record_t), &fr) == sizeof(frame_record_t)) && fr.lr);
}

void dump_thread(thread_t * thread) {
    pt_regs regs;
    verbose("State of thread %s", str_thread(thread));
    if (thread_get_regs(thread, &regs)) {
        state_processor(thread, &regs);
        state_registers(thread, &regs);
        state_backtrace(thread, &regs);
        state_stack(thread, &regs, 16);
    } else {
        verbose("Thread is dead.");
    }
    verbose("End of state of thread %s", str_thread(thread));
}

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"

#include "procutil/mem.h"
#include "tracepoint/patch.h"

#ifndef PSR_F_BIT
#define PSR_F_BIT (1 << 6)
#endif

#ifndef PSR_I_BIT
#define PSR_I_BIT (1 << 7)
#endif

#ifndef PSR_A_BIT
#define PSR_A_BIT (1 << 8)
#endif

#ifndef PSR_E_BIT
#define PSR_E_BIT (1 << 9)
#endif

static void state_processor(thread_t * thread, pt_regs * regs) {

    const char * endianess_str(regval_t cpsr) {
        return (cpsr & PSR_E_BIT) ? "big endian" : "little endian";
    }
    
    const char * exceptions_str(regval_t cpsr) {
        const char * enabled_str(regval_t val) {
            return val ? "enabled" : "disabled";
        }
        static __thread char buf[64];
        
        uint32_t aabt = ((cpsr & PSR_A_BIT) == 0);
        uint32_t fiq = ((cpsr & PSR_I_BIT) == 0);
        uint32_t irq = ((cpsr & PSR_F_BIT) == 0);
        
        uint32_t irqs = fiq && irq;
        uint32_t no_irqs = !fiq && !irq;
        uint32_t all = irqs && aabt;
        
        if (all)
            return "all enabled";
            
        if (irqs)
            return "interrupts enabled, async abort disabled";
            
        if (no_irqs)
            return "interrupts disabled, async abort enabled";
            
        snprintf(buf, 64, "async abort %s, IRQ %s, FIQ %s", enabled_str(aabt),
                 enabled_str(irq), enabled_str(fiq));
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
    
    const char * mode_str(regval_t cpsr) {
        uint32_t mode = cpsr & MODE_MASK;
        switch (mode) {
            case USR_MODE:
                return "usr";
            case FIQ_MODE:
                return "fiq";
            case SVC_MODE:
                return "svc";
            case ABT_MODE:
                return "abt";
            case UND_MODE:
                return "und";
            case SYSTEM_MODE:
                return "sys";
            default:
                return "???";
        }
    }
    
    const char * cond_str(regval_t cpsr) {
        static __thread char buf[] = "     ";
        buf[0] = cpsr & PSR_N_BIT ? 'N' : ' ';
        buf[1] = cpsr & PSR_Z_BIT ? 'Z' : ' ';
        buf[2] = cpsr & PSR_C_BIT ? 'C' : ' ';
        buf[3] = cpsr & PSR_V_BIT ? 'V' : ' ';
        buf[4] = cpsr & PSR_Q_BIT ? 'Q' : ' ';
        return buf;
    }
    
    const char * state_str(regval_t cpsr) {
        if (cpsr & PSR_J_BIT) {
            return (cpsr & PSR_T_BIT) ? "ThumbEE" : "Jazelle";
        } else {
            return (cpsr & PSR_T_BIT) ? "Thumb" : "ARM";
        }
    }
    
    regval_t cpsr = regs->ARM_cpsr;
    address_t ip = instruction_pointer(regs);
    verbose("Processor state:");
    verbose(" Instruction pointer: %s", str_address(thread->process, ip));
    verbose(" State:               %s", state_str(cpsr));
    verbose(" Mode:                %s", mode_str(cpsr));
    verbose(" Exceptions:          %s", exceptions_str(cpsr));
    verbose(" Endianess:           %s", endianess_str(cpsr));
    verbose(" Condition flags:     [%s]", cond_str(cpsr));
    verbose(" IT:                  %s", it_str(cpsr));
    
    {
        insn_kind_t kind = (cpsr & PSR_T_BIT) ? INSN_KIND_THUMB : INSN_KIND_ARM;
        insn_t insn;
        if (arch_check_align(ip, kind) && patch_read_insn_detect_kind(thread, ip, &insn, &kind))
            if (kind == INSN_KIND_THUMB)
                verbose(" Instruction:         %04x\t%s", insn, arm_disassemble(insn, kind));
            else if (kind == INSN_KIND_THUMB2)
                verbose(" Instruction:         %04x %04x\t%s", thumb2_first_halfowrd(insn),
                        thumb2_last_halfowrd(insn), arm_disassemble(insn, kind));
            else
                verbose(" Instruction:         %08x\t%s", insn, arm_disassemble(insn, kind));
        else if (mem_read(thread, ip, 4, &insn) == 4)
            verbose(" Instruction:         %08x\t(invalid alignment)", insn);
        else
            verbose(" Instruction:         unknown\t(invalid alignment)");
    }
}

static void state_registers(thread_t * thread, pt_regs * regs) {

    process_t * process = thread->process;
    
    verbose("Register values:");
#define print_reg(reg)                                          \
    verbose(" %4s  %s",                                         \
            # reg,                                              \
            str_address(process, regs->ARM_ ## reg))
    
    print_reg(r0);
    print_reg(r1);
    print_reg(r2);
    print_reg(r3);
    print_reg(r4);
    print_reg(r5);
    print_reg(r6);
    print_reg(r7);
    print_reg(r8);
    print_reg(r9);
    print_reg(r10);
    print_reg(fp);
    print_reg(ip);
    print_reg(sp);
    print_reg(lr);
    print_reg(pc);
    print_reg(cpsr);
    
#undef print_reg
}

static void state_stack(thread_t * thread, pt_regs * regs, size_t count) {

    regval_t sp = regs->ARM_sp;
    size_t i;
    verbose("Stack:");
    for (i = 0; i < count; ++i) {
        uint32_t val;
        if (mem_read(thread, sp, 4, &val) == 4)
            verbose(" %2u  sp+%02x  0x%08x  %s",
                    (unsigned int) i,
                    (unsigned int) i * 4,
                    sp,
                    str_address(thread->process, val));
        else
            verbose(" %2u  sp+%02x  0x%08x  ???",
                    (unsigned int) i,
                    (unsigned int) i * 4,
                    sp);
        sp += 4;
    }
    
}

void dump_thread(thread_t * thread) {
    pt_regs regs;
    verbose("State of thread %s", str_thread(thread));
    if (thread_get_regs(thread, &regs)) {
        state_processor(thread, &regs);
        state_registers(thread, &regs);
        state_stack(thread, &regs, 16);
    } else {
        verbose("Thread is dead.");
    }
    verbose("End of state of thread %s", str_thread(thread));
}

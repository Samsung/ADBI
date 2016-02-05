#include "tracepoint/template.h"
#include "templates/templates.h"

#include "util/bitops.h"
#include "match.h"

static inline int is_pc(insn_t reg) { return reg == 0xf; }

#define reg(a) bits(insn, a, a + 3)

/**********************************************************************************************************************/

static const template_t * decode_arm_dataproc(insn_t insn) {

    insn_t opcode = bits(insn, 21, 24);
    insn_t rd = reg(12);
    insn_t rn = reg(16);
    insn_t rm = reg(0);
    int uses_rd, uses_rn;
    
    switch (opcode) {
        case 0b1000:    /* tst */
        case 0b1001:    /* teq */
        case 0b1010:    /* cmp */
        case 0b1011:    /* cmn */
            uses_rd = 0;
            uses_rn = 1;
            break;
        case 0b1101:    /* mov */
        case 0b1111:    /* mvn */
            uses_rd = 1;
            uses_rn = 0;
            break;
        default:
            uses_rn = 1;
            uses_rd = 1;
            break;
    }
    
    int immediate = bit(insn, 25);
    
    if (immediate) {
    
        if (!uses_rn) {
            /* the instruction does not use the rn */
            /* mov r0, #123 */
            return &template_arm_generic;
        }
        
        if (!is_pc(rn)) {
            /* rn != pc
             * add r0, r1, r2, #123 */
            return &template_arm_generic;
        }
        
        if (!uses_rd) {
            /* rd unused, rn == pc */
            /* tst pc, #123 */
            return &template_arm_dp_rn2;
        }
        
        if (is_pc(rd)) {
            /* rd == rn == pc
             * add pc, pc, pc, #123 */
            return &template_arm_dp_rd_rn;
        } else {
            /* add r0, pc, #123
             * rn == pc, rd != pc */
            return &template_arm_dp_rn;
        }
        
    } else {
        /* register operand */
        if (!uses_rn) {
            /* mov, mvn */
            if (is_pc(rm)) {
                if (is_pc(rd)) {
                    /* mov pc, pc */
                    return &template_arm_dp_rd_rm;
                } else {
                    /* mov r0, pc */
                    return &template_arm_dp_rm;
                }
            } else {
                /* mov pc, r0 */
                /* mov r0, r1 */
                return &template_arm_generic;
            }
        }
        
        if (!uses_rd) {
            /* tst/cmp... */
            if (is_pc(rn)) {
                if (is_pc(rm)) {
                    /* tst pc, pc */
                    return &template_arm_dp_rn_rm2;
                } else {
                    /* tst pc, r0 */
                    return &template_arm_dp_rn2;
                }
            } else {
                if (is_pc(rm)) {
                    /* tst pc, pc */
                    return &template_arm_dp_rm2;
                } else {
                    /* tst pc, r0 */
                    return &template_arm_generic;
                }
            }
        }
        
        /* the instruction uses both rn and rd */
        
        if (is_pc(rd)) {
            if (is_pc(rn)) {
                if (is_pc(rm)) {
                    /* add pc, pc, pc */
                    return &template_arm_dp_rd_rn_rm;
                } else {
                    /* add pc, pc, r1 */
                    return &template_arm_dp_rd_rn;
                }
            } else {
                if (is_pc(rm)) {
                    /* add pc, r0, r1 */
                    return &template_arm_dp_rd_rm;
                } else {
                    /* add pc, r0, r1 */
                    return &template_arm_generic;
                }
            }
        } else {
            if (is_pc(rn)) {
                if (is_pc(rm)) {
                    /* add r0, pc, pc */
                    return &template_arm_dp_rn_rm;
                } else {
                    /* add r0, pc, p2 */
                    if (rd == 13)
                        return NULL;
                    return (rd != rm) ? &template_arm_dp_rn : &template_arm_dp_rn2;
                }
            } else {
                if (is_pc(rm)) {
                    /* add r0, r1, pc */
                    if (rd == 13)
                        return NULL;
                    return (rd != rn) ? &template_arm_dp_rm : &template_arm_dp_rm2;
                } else {
                    /* add r0, r1, r2 */
                    return &template_arm_generic;
                }
            }
        }
    }
}

static const template_t * decode_arm_load_store(insn_t insn) {

    insn_t load = bit(insn, 20);
    insn_t w = bit(insn, 21);
    insn_t p = bit(insn, 24);
    
    int writeback = !p || w;
    
    insn_t rn = reg(16);        /* base register */
    insn_t rd = reg(12);        /* transferred register */
    
    /* Using pc as the base register with writeback enabled is unpredictable */
    if (writeback && is_pc(rn)) {
        return NULL;
    }
    
    if (bit(insn, 25)) {
        /* Register or scaled register offset/index (uses rm) */
        insn_t rm = reg(0); /* offset/index register */
        if (is_pc(rm))
            return NULL;            /* unpredictable */
        if (rm == rn)
            return NULL;            /* unpredictable */
    } else {
        /* Immediate offset/index */
    }
    
    if ((rd == rn) && is_pc(rn)) {
        /* ldr pc, [pc, ...] */
        return  NULL;   /* uncommon (usually used for branching to absolute addresses) */
    }
    
    if (!load && is_pc(rd)) {
        return NULL;    /* uncommon (e.g. str pc, [r0]) */
    }
    
    /* At this point we know that rm != pc; rn != rd */
    
    if (is_pc(rn))
        return &template_arm_dp_rn; /* load literal, load pc-relative */
    else {
        /* Note that this also handles lrd pc, rx... properly */
        return &template_arm_generic;
    }
    
}

static const template_t * decode_arm_load_store_multiple(insn_t insn) {
    insn_t regs = bits(insn, 0, 15);    /* register list */
    insn_t rn = reg(16);                /* base register */
    insn_t load = bit(insn, 20);
    insn_t writeback = bit(insn, 21);
    
    if (!regs) {
        /* no registers specified */
        return NULL;    /* unpredictable */
    }
    
    if (is_pc(rn)) {
        /* load/store multiple, pc-relative */
        return NULL;    /* uncommon */
    }
    
    if (load) {
        /* load */
        if (bit(regs, rn) && writeback) {
            /* rn is in the list of registers to load and writeback is enabled */
            return NULL;    /* unpredictable */
        }
    } else {
        /* store */
        if (bit(regs, 15)) {
            /* stores pc */
            return NULL;    /* uncommon, behavior is implementation defined */
        }
    }
    
    return &template_arm_generic;
}

static const template_t * decode_arm_load_store_extra(insn_t insn) {

    insn_t rn = reg(16);
    insn_t rd = reg(12);
    
    insn_t immediate = bit(insn, 22);
    
    if (rd == rn) {
        return NULL;    /* unpredictable */
    }
    
    if (is_pc(rd)) {
        return NULL;    /* unpredictable */
    }
    
    if (!immediate) {
        /* register addressing */
        insn_t rm = reg(0);
        
        if (is_pc(rm)) {
            return NULL;    /* unpredictable */
        }
    }
    
    if (is_pc(rn))
        return &template_arm_dp_rn;
    else
        return &template_arm_generic;
}

static const template_t * decode_arm_load_store_dual(insn_t insn) {
    insn_t rd = reg(12);
    if (rd & 0x1) {
        return NULL;    /* undefined */
    }
    if (rd == 14) {
        return NULL;    /* unpredictable */
    }
    return decode_arm_load_store_extra(insn);
}

static const template_t * decode_arm_nopc(insn_t insn, int uses_rn, int uses_rd, int uses_rs, int uses_rm) {
    insn_t rm = reg(0);
    insn_t rs = reg(8);
    insn_t rd = reg(12);
    insn_t rn = reg(16);
    
    if (uses_rn && is_pc(rn)) return NULL;
    if (uses_rd && is_pc(rd)) return NULL;
    if (uses_rs && is_pc(rs)) return NULL;
    if (uses_rm && is_pc(rm)) return NULL;
    
    return &template_arm_generic;
}

static const template_t * decode_arm_branch(insn_t insn) {
    insn_t rm = reg(0);
    
    if (is_pc(rm))
        return NULL;
        
    if (bit(insn, 5))
        return &template_blx_reg_a1;
    else
        return &template_arm_generic;
}

const template_t * template_select_arm(insn_t insn)  {

    if (match(insn, "11110---------------------------")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "1111100-------------------------")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "11111111------------------------")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "1111101-------------------------")) {
        /* branch with link and change to thumb */
        return &template_blx_imm_a2;
    }
    
    if (match(insn, "1111----------------------------")) {
        /* unconditional instruction */
        return NULL;
    }
    
    if (match(insn, "----00110-00--------------------")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "----011--------------------1----")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "----00010-00------------0000----")) {
        /* move status register to register */
        return decode_arm_nopc(insn, 0, 1, 0, 1);
    }
    
    if (match(insn, "----00010-10------------0000----")) {
        /* move register to status register */
        return decode_arm_nopc(insn, 0, 0, 0, 1);
    }
    
    if (match(insn, "----00010010------------0001----")) {
        /* branch and exchange */
        return decode_arm_branch(insn);
    }
    
    if (match(insn, "----00010110------------0001----")) {
        /* count leading zeros */
        return decode_arm_nopc(insn, 0, 1, 0, 1);
    }
    
    if (match(insn, "----00010010------------0011----")) {
        /* branch and link/exchange instruction set */
        return decode_arm_branch(insn);
    }
    
    if (match(insn, "----00010--0------------0101----")) {
        /* enhanced DSP add/substract */
        return NULL;
    }
    
    if (match(insn, "----000--------------------0----")) {
        /* data processing immediate shift */
        return decode_arm_dataproc(insn);
    }
    
    if (match(insn, "----000-----------------0--1----")) {
        /* data processing register shift */
        return decode_arm_dataproc(insn);
    }
    
    if (match(insn, "----001-------------------------")) {
        /* data processing immediate */
        return decode_arm_dataproc(insn);
    }
    
    if (match(insn, "----010-------------------------")) {
        /* load/store immediate offset */
        return decode_arm_load_store(insn);
    }
    
    if (match(insn, "----011--------------------0----")) {
        /* load/store register offset */
        return decode_arm_load_store(insn);
    }
    
    if (match(insn, "----100-------------------------")) {
        /* load/store multiple offset */
        return decode_arm_load_store_multiple(insn);
    }
    
    if (match(insn, "----1010------------------------")) {
        /* branch */
        return &template_b_a1;
    }
    
    if (match(insn, "----1011------------------------")) {
        /* branch with link */
        return &template_bl_imm_a1;
    }
    
    if (match(insn, "----00010010------------0111----")) {
        /* software breakpoint */
        return NULL;
    }
    
    if (match(insn, "----00010--0------------1--0----")) {
        /* enhanced DSP multiply */
        return NULL;
    }
    
    if (match(insn, "----000000--------------1001----")) {
        /* multiply (accumulate) */
        return decode_arm_nopc(insn, 1, 1, 1, 1);
    }
    
    if (match(insn, "----00001---------------1001----")) {
        /* multiply (accumulate) long */
        return decode_arm_nopc(insn, 1, 1, 1, 1);
    }
    
    if (match(insn, "----00010-00------------1001----")) {
        /* swap/swap byte */
        return decode_arm_nopc(insn, 1, 1, 0, 1);
    }
    
    if (match(insn, "----00011---------------1001----")) {
        /* load/store register exclusive word/doubleword/byte/halfword */
        return decode_arm_nopc(insn, 1, 1, 0, 1);
    }
    
    if (match(insn, "----000--0--------------1011----")) {
        /* load/store halfword register offset */
        return decode_arm_load_store_extra(insn);
    }
    
    if (match(insn, "----000--1--------------1011----")) {
        /* load/store halfword immediate offset */
        return decode_arm_load_store_extra(insn);
    }
    
    if (match(insn, "----000--0-1------------11-1----")) {
        /* load/store signed halfword/byte register offset */
        return decode_arm_load_store_extra(insn);
    }
    
    if (match(insn, "----000--1-1------------11-1----")) {
        /* load/store signed halfword/byte immediate offset */
        return decode_arm_load_store_extra(insn);
    }
    
    if (match(insn, "----000--0-0------------11-1----")) {
        /* load/store dual register offset */
        return decode_arm_load_store_dual(insn);
    }
    
    if (match(insn, "----000--1-0------------11-1----")) {
        /* load/store dual immediate offset */
        return decode_arm_load_store_dual(insn);
    }
    
    if (match(insn, "----1111------------------------")) {
        /* software interrupt */
        return &template_arm_generic;
    }
    
    return NULL;
}

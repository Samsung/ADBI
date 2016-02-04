#include "tracepoint/template.h"
#include "templates/templates.h"

#include "util/bitops.h"

#define bitsh(low, high)    bits(insn, low + 16, high + 16)
#define bitsl(low, high)    bits(insn, low, high)
#define bith(n)             bit(insn, n + 16)
#define bitl(n)             bit(insn, n)

#include "match.h"

static inline int is_sp(insn_t reg) { assert(reg <= 0xf); return reg == 0xd; }
static inline int is_pc(insn_t reg) { assert(reg <= 0xf); return reg == 0xf; }
static inline int is_bad(insn_t reg) { return is_sp(reg) || is_pc(reg); }
static inline int is_high(insn_t reg) { assert(reg <= 0xf); return reg >= 0x8; }
static inline int is_low(insn_t reg) { assert(reg <= 0xf); return !is_high(reg); }

static const template_t * template_select_dataproc_mim(insn_t insn) {
    /* ARMv7 ARM: A6.3.1 */
    assert(match(insn, "11110-0---------0---------------"));
    
    insn_t op = bitsh(5, 8);
    insn_t s = bith(4);
    insn_t rn = bitsh(0, 3);
    insn_t rd = bitsl(8, 11);
    
    if (match(op, "-000") || match(op, "1101") || match(op, "0-00")) {
        /* and/tst, eor/teq, add/cmn, sub/cmp */
        if (is_bad(rn))
            return NULL;        /* unpredictable */
        if (is_pc(rd) && !s)    /* unpredictable */
            return NULL;
        if (is_sp(rd))          /* unpredictable */
            return NULL;
        return &template_thumb2_generic;
    } else if (match(op, "101-") || match(op, "000-") || match(op, "1-10")) {
        /* regular data processing instruction using 2 registers - rn and rd, but none of them can be sp or pc */
        if (is_bad(rd) || is_bad(rn))
            return NULL;
        return &template_thumb2_generic;
    } else if (match(op, "00--")) {
        /* bitwise bit orr/mov and orn/mvn */
        if (is_bad(rd))
            return NULL;        /* unpredictable */
        /* if rn == pc, a different instruction is encoded (e.g. or becomes mov); still rn must not be sp */
        if (is_sp(rn))
            return NULL;        /* unpredictable */
    }
    
    /* undefined */
    return NULL;
    
}

static const template_t * template_select_dataproc_imm(insn_t insn) {
    /* ARMv7 ARM: A6.3.3 */
    assert(match(insn, "11110-1---------0---------------"));
    
    insn_t op = bitsh(4, 8);
    insn_t rn = bitsh(0, 3);
    insn_t rd = bitsl(8, 11);
    
    if (is_bad(rd))     /* unpredictable */
        return NULL;
        
    if (match(op, "00000") || match(op, "01010")) {
        if (is_pc(rn)) {
            /* adr */
            if (match(op, "00000")) {
                /* encoding t3 */
                return &template_adr_t3;
            } else {
                /* encoding t2 */
                return &template_adr_t2;
            }
        } else {
            /* add/sub */
            return &template_thumb2_generic;
        }
    } else if (match(op, "--100") || match(op, "1-0-0")) {
        /* regular instruction */
        if (is_bad(rn))     /* unpredictable */
            return NULL;
        else
            return &template_thumb2_generic;
    } else if (match(op, "10--0")) {
        /* bfi/bfc -- rn can be pc */
        if (is_sp(rn))  /* unpredictable */
            return NULL;
        return &template_thumb2_generic;
    } else {
        /* other encodings are undefined */
        return NULL;
    }
}

static const template_t * template_select_branch_misc_ctrl(insn_t insn) {
    /* ARMv7 ARM: A6.3.4 */
    assert(match(insn, "11110-----------1---------------"));
    
    insn_t op = bitsh(4, 10);
    insn_t op1 = bitsl(12, 14);
    insn_t op2 = bitsl(8, 11);
    
    switch (op1) {
        case 0b000:
        case 0b010:
            /* 0x0   ... */
            switch (op) {
                case 0b0111000:
                    /* move to special register */
                    if (match(op2, "--00")) {
                        /* move to special register -- application level */
                        insn_t rn = bitsh(0, 4);
                        if (is_bad(rn))     /* unpredictable */
                            return NULL;
                        return &template_thumb2_generic;
                    }
                /* else: fall through */
                case 0b0111001:
                    /* move to special register -- system level */
                    return NULL;
                case 0b0111010: {
                        insn_t op1 = bitsl(8, 10);
                        if (op1) {
                            /* hint */
                            return &template_thumb_nop;
                        } else {
                            /* change processor state -- legal only in privileged modes */
                            return NULL;
                        }
                    }
                case 0b0111011:
                    /* TODO: miscellaneous control instructions */
                    return NULL;
                case 0b111100:
                    /* TODO: branch and exchange jazelle */
                    return NULL;
                case 0b0111101:
                    /* exception return -- legal only in privileged modes */
                    return NULL;
                case 0b0111110:
                case 0b0111111: {
                        /* move from special register  */
                        if (!match(insn, "11110011111011111000----00000000"))   /* unpredictable */
                            return NULL;
                            
                        insn_t rd = bitsl(8, 11);
                        if (is_bad(rd))     /* unpredictable */
                            return NULL;
                            
                        return &template_thumb2_generic;
                    }
                case 0b1111111:
                    if (op1 == 0b010) {
                        /* permanently undefined */
                        return NULL;
                    } else {
                        /* secure monitor call -- legal only in privileged modes */
                        return NULL;
                    }
                    break;
                default:
                    assert(!match(op, "-111---"));
                    /* conditional branch (b t3) */
                    return &template_b_t3;
            }
            break;
            
        case 0b001:
        case 0b011:
            /* 0x1   unconditional branch  */
            return &template_b_t4;
        case 0b100:
        case 0b110:
            /* 1x0   branch with link and exchange */
            return &template_blx_t2;
        case 0b101:
        case 0b111:
            /* 1x1   branch with link */
            return &template_bl_t1;
    }
    return NULL;
}

static const template_t * template_select_ldm_stm(insn_t insn) {
    /* ARMv7 ARM: A6.3.5 */
    assert(match(insn, "1110100--0----------------------"));
    
    insn_t op = bitsh(7, 8);
    insn_t rn = bitsh(0, 3);
    
    insn_t regs = bitsl(0, 15);
    
    insn_t w = bith(5);
    insn_t l = bith(4);
    
    insn_t sp = regs & (1 << 13);
    insn_t lr = regs & (1 << 14);
    insn_t pc = regs & (1 << 15);
    
    if ((op == 0b00) || (op == 0b11)) {
        /* return from exception or store return state */
        return NULL;
    }
    
    if (bits_count(regs) < 2)       /* unpredictable */
        return NULL;
        
    if (is_pc(rn))                      /* unpredictable */
        return NULL;
        
    if (l) {
        /* ldm or pop */
        if ((pc && lr) || sp)           /* unpredictable */
            return NULL;
    } else {
        /* stm or push */
        if (pc || sp)                   /* unpredictable */
            return NULL;
    }
    
    if (w && (regs & (1 << rn)))       /* unpredictable */
        return NULL;
        
    return &template_thumb2_generic;
    
}

static const template_t * template_select_ldr(insn_t insn) {
    /* ARMv7 ARM: A6.3.7 */
    assert(match(insn, "1111100--101--------------------"));
    
    insn_t op1 = bitsh(7, 8);
    insn_t op2 = bitsl(6, 11);
    
    insn_t rn = bitsh(0, 3);
    insn_t rt = bitsl(12, 15);
    
    if (!match(op1, "0-"))  /* undefined */
        return NULL;
        
    if (is_pc(rn)) {
        /* load literal, t2 */
        if (!is_pc(rt)) {
            return &template_ldr_lit_t2;
        } else {
            /* TODO: load literal to pc */
            return NULL;
        }
    } else {
        if ((op1 == 0b00) && (op2 == 0b000000)) {
            /* load register (register), t2 */
            insn_t rm = bitsl(0, 3);
            if (is_bad(rm))     /* unpredictable */
                return NULL;
            return &template_thumb2_generic;
        }
        
        if (op1 == 0b01) {
            /* load register (immediate), t3 */
            return &template_thumb2_generic;
        }
        
        if ((op1 == 0b00) && (match(op2, "1--1--") || match(op2, "1100--"))) {
            /* load register (immediate), t4 or load register unprivileged */
            
            insn_t p = bitl(10);
            insn_t u = bitl(9);
            insn_t w = bitl(8);     /* writeback */
            
            if (!p && !w)           /* undefined */
                return NULL;
                
            if (w && (rn == rt))    /* unpredictable */
                return NULL;
                
            if (p && u && !w) {
                /* unprivileged */
                if (is_bad(rt))     /* unpredictable */
                    return NULL;
            }
            
            return &template_thumb2_generic;
        }
        
    }
    
    /* undefined */
    return NULL;
}

static const template_t * template_select_str(insn_t insn) {
    /* ARMv7 ARM: A6.3.10 */
    assert(match(insn, "11111000---0--------------------"));
    
    insn_t op1 = bitsh(5, 7);
    insn_t op2 = bitsl(6, 11);
    
    insn_t rn = bitsh(0, 3);
    insn_t rt = bitsl(12, 15);
    
    /* Instructions covered: str/strh/strb */
    if (is_pc(rn))      /* unpredictable */
        return NULL;
        
    /* the str instruction can store the sp, but not pc; other instructions can not store */
    if (match(op1, "-10")) {
        /* str */
        if (is_pc(rt))      /* unpredictable */
            return NULL;
    } else {
        /* strh or strb (or undefined) */
        if (is_bad(rt))     /* unpredictable */
            return NULL;
    }
    
    if (match(op1, "00-") || match(op1, "0-0")) {
    
        if (match(op2, "000000")) {
            /* register */
            insn_t rm = bitsl(0, 3);
            
            if (is_bad(rm))
                return NULL;
                
            return &template_thumb2_generic;
            
        } else if (match(op2, "1--1--") || match(op2, "11----")) {
            /* immediate (encoding t4) or unprivileged (*t variants) */
            
            insn_t p = bitl(10);
            /* insn_t u = bitl(9); */
            insn_t w = bitl(8);     /* writeback */
            
            if (!p && !w)           /* undefined */
                return NULL;
                
            if (w && (rn == rt))    /* upredictable */
                return NULL;
                
            return &template_thumb2_generic;
        }
        
    } else if (match(op1, "-0-") ||
               match(op1, "--0")) {
        /* immediate (encoding t3) */
        
        /* no further checks required */
        return &template_thumb2_generic;
    }
    
    /* undefined */
    return NULL;
}

static const template_t * template_select_dataproc_shr(insn_t insn) {
    /* ARMv7 ARM: A6.3.11 */
    assert(match(insn, "1110101-------------------------"));
    
    insn_t op = bitsh(5, 8);
    insn_t s = bith(4);
    insn_t rn = bitsh(0, 3);
    insn_t rd = bitsl(8, 11);
    insn_t rm = bitsl(0, 3);
    
    if (match(op, "-000") || match(op, "1101") || match(op, "0-00")) {
        /* and/tst, eor/teq, add/cmn, sub/cmp */
        if (is_bad(rn) || is_bad(rm))
            return NULL;    /* unpredictable */
        if (is_pc(rd) && !s)
            return NULL;    /* unpredictable */
        /* if rd == pc and s is set, a different instruction is encoded (e.g. and becomes tst); still rd must not
         * be sp */
        if (is_sp(rd))      /* unpredictable */
            return NULL;
        return &template_thumb2_generic;
    } else if (match(op, "101-") || match(op, "000-") || match(op, "-110")) {
        /* regular data processing instruction using 3 registers - rm, rn, rd, but none of them can be sp or pc */
        if (is_bad(rd) || is_bad(rm) || is_bad(rn))
            return NULL;
        return &template_thumb2_generic;
    } else if (match(op, "-01-")) {
        /* bitwise bit orr/mov and orn/mvn */
        if (is_bad(rd) || is_bad(rm))
            return NULL;        /* unpredictable */
        /* if rn == pc, a different instruction is encoded (e.g. or becomes mov); still rd must not be sp */
        if (is_sp(rn))
            return NULL;        /* unpredictable */
    }
    
    /* undefined */
    return NULL;
}

static const template_t * template_select_dataproc_reg(insn_t insn) {
    /* ARMv7 ARM: A6.3.12 */
    assert(match(insn, "11111010------------------------"));
    
    if (bitsl(12, 15) != 0b1111)    /* undefined */
        return NULL;
        
    insn_t rn = bitsh(0, 3);
    insn_t rd = bitsl(8, 11);
    insn_t rm = bitsl(0, 3);
    
    insn_t op1 = bitsh(4, 7);
    insn_t op2 = bitsl(4, 7);
    
    if (match(op1, "0---") && match(op2, "0000")) {
        /* lsl, lsr asr, ror */
        if (is_bad(rd) || is_bad(rm) || is_bad(rn))
            return NULL;
        return &template_thumb2_generic;
    }
    
    if (match(op2, "1---") && (op1 <= 0b0101)) {
        /* signed/unsigned extend */
        if (is_bad(rd) || is_bad(rm))
            return NULL;
        return &template_thumb2_generic;
    }
    
    if (match(op1, "1---") && match(op2, "0---")) {
        /* ARMv7 ARM: A6.3.13-14 */
        /* parallel addition and subtraction, unsigned */
        assert(match(insn, "111110101-------1111----0-------"));
        
        insn_t op1 = bitsh(4, 6);
        insn_t op2 = bitsl(4, 5);
        
        if (op2 == 0b11)        /* undefined */
            return NULL;
            
        if (match(op1, "-11"))  /* undefined */
            return NULL;
            
        if (is_bad(rd) || is_bad(rm) || is_bad(rn))     /* unpredictable */
            return NULL;
            
        return &template_thumb2_generic;
    }
    
    if (match(op1, "10--") && match(op2, "10--")) {
        /* ARMv7 ARM: A6.3.15 */
        /* miscellaneous operations */
        assert(match(insn, "1111101010------1111----10------"));
        
        insn_t op1 = bitsh(4, 5);
        insn_t op2 = bitsl(4, 5);
        
        if (match(op1, "1-") && (op2 != 0b00))  /* undefined */
            return NULL;
            
        if (is_bad(rd) || is_bad(rm) || is_bad(rn))     /* unpredictable */
            return NULL;
            
        return &template_thumb2_generic;
    }
    
    /* anything else is undefined */
    return NULL;
}

static const template_t * template_select_mul(insn_t insn) {
    /* ARMv7 ARM: A6.3.16 */
    assert(match(insn, "111110110-----------------------"));
    
    if (bitsl(6, 7) != 0b00)    /* undefined */
        return NULL;
        
    insn_t op1 = bitsh(4, 6);
    insn_t op2 = bitsl(4, 5);
    insn_t op12 = (op1 << 2) | op2;
    
    insn_t rn = bitsh(0, 3);
    insn_t rm = bitsl(0, 3);
    insn_t ra = bitsl(12, 15);
    insn_t rd = bitsl(8, 11);
    
    if (is_pc(rn) || is_pc(rm) || is_pc(ra) || is_pc(rd))   /* unpredictable */
        return NULL;
        
    if ((match(op12, "001--") || match(op12, "---0-")) && !match(op12, "11101")) {
        return &template_thumb2_generic;
    } else {
        /* undefined */
        return NULL;
    }
    
}

static const template_t * template_select_mull(insn_t insn) {
    /* ARMv7 ARM: A6.3.17 */
    assert(match(insn, "111110111-----------------------"));
    
    insn_t op1 = bitsh(4, 6);
    insn_t op2 = bitsl(4, 7);
    insn_t op12 = (op1 << 3) | op2;
    
    insn_t rn = bitsh(0, 3);
    insn_t rm = bitsl(0, 3);
    insn_t rdlo = bitsl(12, 15);
    insn_t rdhi = bitsl(8, 11);
    
    if (is_pc(rn) || is_pc(rm) || is_pc(rdlo) || is_pc(rdhi))   /* unpredictable */
        return NULL;
        
    if (match(op12, "10-110-") ||
            match(op12, "1100110") ||
            match(op12, "10010--") ||
            match(op12, "--00000") ||
            match(op12, "0-11111")) {
        /* defined */
        return &template_thumb2_generic;
    } else {
        /* undefined */
        return NULL;
    }
    
}

const template_t * template_select_thumb32(insn_t insn) {

    assert(is_thumb2_halfword(thumb2_first_halfowrd(insn)));
    
    insn_t op1 = bitsh(11, 12);
    insn_t op2 = bitsh(4, 10);
    insn_t op = bitl(15);
    
    switch (op1) {
        case 0b01:
            if (match(op2, "1------")) {
                /* TODO: coprocessor instructions */
                return NULL;
            } else if (match(op2, "01-----")) {
                /* data processing (shifted register) */
                return template_select_dataproc_shr(insn);
            } else if (match(op2, "00--0--")) {
                /* load/store multiple */
                return template_select_ldm_stm(insn);
            } else if (match(op2, "00--1--")) {
                /* TODO: load/store dual/exclusive, table branch */
                return NULL;
            } else {
                return NULL;
            }
        case 0b10:
            if (op) {
                /* branches and misc control */
                return template_select_branch_misc_ctrl(insn);
            } else {
                if (match(op2, "-0-----")) {
                    /* data processing (modified immediate) */
                    return template_select_dataproc_mim(insn);
                } else {
                    /* data processing (plain binary immediate) */
                    return template_select_dataproc_imm(insn);
                }
            }
        case 0b11:
            if (match(op2, "000---0")) {
                /* store single data item */
                return template_select_str(insn);
            } else if (match(op2, "001---0")) {
                /* TODO: advanced simd element or structure load instructions */
                return NULL;
            } else if (match(op2, "00--001")) {
                /* TODO: load byte, memory hints */
                return NULL;
            } else if (match(op2, "00--011")) {
                /* TODO: load halfword */
                return NULL;
            } else if (match(op2, "00--101")) {
                /* load word */
                return template_select_ldr(insn);
            } else if (match(op2, "010----")) {
                /* data processing (register) */
                return template_select_dataproc_reg(insn);
            } else if (match(op2, "0110---")) {
                /* multiply, multiply accumulate, and absolute difference */
                return template_select_mul(insn);
            } else if (match(op2, "0111---")) {
                /* long multiply, long multiply accumulate, and divide */
                return template_select_mull(insn);
            } else if (match(op2, "1------")) {
                /* TODO: coprocessor instructions */
                return NULL;
            } else {
                /* undefined */
                return NULL;
            }
        default:
            /* 16-bit instruction */
            assert(0);
    }
    
    
    return NULL;
}


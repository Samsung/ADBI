#include "tracepoint/template.h"
#include "templates/templates.h"

#include "util/bitops.h"
#include "match.h"

static inline int is_pc(insn_t reg) { assert(reg <= 0xf); return reg == 0xf; }
static inline int is_high(insn_t reg) { assert(reg <= 0xf); return reg >= 0x8; }
static inline int is_low(insn_t reg) { assert(reg <= 0xf); return !is_high(reg); }

static const template_t * decode_thumb_ldm_stm(insn_t insn) {
    /* registers to load or store */
    insn_t register_list = insn & 0xf;
    insn_t rn = (insn >> 8) & 0x7;
    
    if (!register_list) {
        /* no registers specified -- unpredictable */
        return NULL;
    }
    
    if (register_list & (1 << rn)) {
        /* instruction reads or writes the base register -- this is defined for ARMv5, but depreciated in
         * ARMv7. */
        return NULL;
    }
    
    return &template_thumb_generic;
}

static const template_t * decode_thumb_dp_hr_branch(insn_t insn) {

    enum {
        ADD = 0b00,
        CMP = 0b01,
        MOV = 0b10,
        BLX = 0b11,
    } opcode = bits(insn, 8, 9);
    
    insn_t rm = bits(insn, 3, 6);
    insn_t rn = bits(insn, 0, 2) | (bit(insn, 7) << 3);         /* unused in BLX encoding */
    
    switch (opcode) {
        case ADD:
            /* add rn, rm -- note that this instruction reads both operand registers */
            /* Unlike other instructions in this group, two low registers can be used in this case. */
            if (is_pc(rn) && is_pc(rm)) {
                /* both registers are the pc -- unpredictable */
                return NULL;
            }
            if (!is_pc(rn) && !is_pc(rm)) {
                /* None of the registers is the pc. */
                return &template_thumb_generic;
            } else {
                /* TODO: at least one of the registers is pc */
                return NULL;
            }
            
        case CMP:
            if (is_pc(rn) || is_pc(rm)) {
                /* one of the registers is the pc -- unpredictable */
                return NULL;
            }
            if (is_low(rn) && is_low(rm)) {
                /* comparing two low registers -- unpredictable */
                return NULL;
            }
            /* anything else is predictable and can be handled by the generic handler */
            return &template_thumb_generic;
            
        case MOV:
            if (is_pc(rm)) {
                /* TODO: source register is pc */
                return NULL;
            } else {
                /* Source register is not the pc. In this case we can use the generic handler, even if the
                 * destination register is the pc (this will cause a branch without link). */
                return &template_thumb_generic;
            }
            
        case BLX:
            /* branch and change instruction set */
            if (bits(insn, 0, 2)) {
                /* the last 3 bits SBZ */
                return NULL;
            }
            
            if (bit(opcode, 7)) {
                /* branch with link */
                return &template_blx_reg_t1;
            } else {
                /* branch and exchange */
                return &template_thumb_generic;
            }
            
        default:
            adbi_bug_unrechable();
            return NULL;
    }
    
}

const template_t * template_select_thumb(insn_t insn) {

    if (match(insn, "00--------------")) {
        /*
         *      000110----------
         *      add/subtract register
         *
         *      000111----------
         *      add/subtract immediate
         *
         *      000pp-----------, pp != 11
         *      shift by immediate
         *
         *      001-------------
         *      add/subtract/compare/move immediate
         */
        return &template_thumb_generic;
    }
    
    if (match(insn, "010000----------")) {
        /* data processing register */
        return &template_thumb_generic;
    }
    
    if (match(insn, "010001----------")) {
        /*
         *      01000111--------
         *      branch/exchange instruction set
         *
         *      010001pp--------; pp != 11
         *      special data processing (high registers)
         */
        return decode_thumb_dp_hr_branch(insn);
    }
    
    if (match(insn, "01001-----------")) {
        /* load from literal pool */
        return &template_ldr_lit_t1;
    }
    
    if (match(insn, "0101------------")) {
        /* load/store register offset */
        return &template_thumb_generic;
    }
    
    if (match(insn, "011-------------")) {
        /* load/store word/byte immediate offset */
        return &template_thumb_generic;
    }
    
    if (match(insn, "100-------------")) {
        /*
         *      1000------------
         *      load/store halfword register offset
         *
         *      1001------------
         *      load/store to/from stack
         */
        return &template_thumb_generic;
    }
    
    if (match(insn, "10101-----------")) {
        /* add to sp */
        return &template_thumb_generic;
    }
    
    if (match(insn, "10100-----------")) {
        /* add to pc (aka adr: generate pc-relative address) */
        return &template_adr_t1;
    }
    
    if (match(insn, "1011------------")) {
        /* miscellaneous instructions */
        
        if (match(insn, "10110000--------")) {
            /* adjust stack pointer */
            return &template_thumb_generic;
        }
        
        if (match(insn, "1011-10---------")) {
            /* push/pop */
            return &template_thumb_generic;
        }
        
        if (match(insn, "10111110--------")) {
            /* software breakpoint */
            return &template_thumb_generic;
        }
        
        if (match(insn, "1011-0-1--------")) {
            /* compare and branch on (non-)zero */
            return &template_cbz_t1;
        }
        
        return NULL;
    }
    
    if (match(insn, "1100------------")) {
        /* load/store multiple */
        return decode_thumb_ldm_stm(insn);
    }
    
    if (match(insn, "1101------------")) {
    
        if (match(insn, "11011111--------")) {
            /* software interrupt */
            return &template_thumb_generic;
        }
        
        if (match(insn, "11011110--------")) {
            /* permanently undefined */
            return NULL;
        }
        
        /*  1101cccc--------
         *  conditional branch  */
        return &template_b_t1;
    }
    
    if (match(insn, "11101----------1")) {
        /* undefined instruction */
        return NULL;
    }
    
    if (match(insn, "11100-----------")) {
        /* unconditional branch */
        return &template_b_t2;
    }
    
    if (is_thumb2_halfword(insn)) {
        /* this is a 32-bit instruction */
        return NULL;
    }
    
    return NULL;
}

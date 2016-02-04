#include "tracepoint/template.h"

#include "tracepoint/template.h"
#include "templates/templates.h"

#include "util/bitops.h"
#include "match.h"

static const template_t * decode_a64_data_processing_immediate(insn_t insn) {
    if (match(insn, "---10000------------------------")) {
        /* PC-rel. addressing */
        return &template_a64_adrp;  /* adr, adrp */
    };
    return &template_a64_generic;
}

static const template_t * decode_a64_branch_exception_system(insn_t insn) {

    if (match(insn, "-00101--------------------------")) {
        /* Unconditional branch (immediate) */
        int op = bit(insn, 31);
        if (op)
            return &template_a64_bl;
        else
            return &template_a64_b;
    };

    if (match(insn, "-011010-------------------------")) {
        /* Compare & branch (immediate) */
        return &template_a64_cbnz;
    };

    if (match(insn, "-011011-------------------------")) {
        /* Test & branch (immediate) */
        return &template_a64_tbnz;
    };

    if (match(insn, "0101010-------------------------")) {
        /* Conditional branch (immediate) */
        return &template_a64_b_cond;
    };

    if (match(insn, "11010100------------------------")) {
        /* Exception generation */
        return &template_a64_generic;
    };

    if (match(insn, "1101010100----------------------")) {
        /* System */
        return &template_a64_generic;
    };

    if (match(insn, "1101011-------------------------")) {
        /* Unconditional branch (register) */
        insn_t opc = bits(insn, 21,24);
        switch (opc) {
        case 0b0000:    /* br */
        case 0b0010:    /* ret */
            return &template_a64_br;
        case 0b0001:    /* blr */
        {
            insn_t Rn = bits(insn, 5, 9);
            if (Rn == 30)
                return &template_a64_blr_x30;
            else
                return &template_a64_blr;
        }
        case 0b0100:    /* eret */
        case 0b0101:    /* drps */
            return &template_a64_generic;
        }
    };
    return NULL;
}

static const template_t * decode_a64_load_store(insn_t insn) {
    if (match(insn, "--011-00------------------------")) {
        /* Load register (literal) */
        int opc = insn >> 30;
        int v = insn & 0x04000000;
        if ((opc == 0b11) && !(v))
            return NULL;    /* prfm */
        else if ((opc == 0b10) && !(v))
            return &template_a64_ldrsw_literal;

        return &template_a64_ldr_literal;
    }

    return &template_a64_generic;
}

const template_t * template_select_a64(insn_t insn)  {

    if (match(insn, "---00---------------------------")) {
        /* undefined instruction */
        return NULL;
    }

    if (match(insn, "---100--------------------------")) {
        /* Data processing - immediate */
        return decode_a64_data_processing_immediate(insn);
    }

    if (match(insn, "---101--------------------------")) {
        /* Branch, exception generation and system instructions */
        return decode_a64_branch_exception_system(insn);
    }

    if (match(insn, "----1-0-------------------------")) {
        /* Loads and stores */
        return decode_a64_load_store(insn);
    }

    if (match(insn, "----101-------------------------")) {
        /* Data processing - register */
        return &template_a64_generic;
    }

    if (match(insn, "----111-------------------------")) {
        /* Data processing - SIMD and floating point */
        return NULL;
    }

    return NULL;
}

const template_t * template_select(insn_t insn, insn_kind_t kind) {
    switch (kind) {
        case INSN_KIND_T32_16:
        case INSN_KIND_T32_32:
        case INSN_KIND_A32:
        default:
            return NULL;
        case INSN_KIND_A64:
            return template_select_a64(insn);
    }
}

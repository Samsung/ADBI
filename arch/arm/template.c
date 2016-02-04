#include "tracepoint/template.h"
#include "match.h"

#include "util/bitops.h"

typedef struct template_values_t {
    address_t handler_address;
    address_t pc;
    address_t next_pc;
    insn_t insn;
    insn_kind_t insn_kind;
} template_values_t;

typedef struct insn_masks_t {
    insn_t cond, rd, rt, rm, rn;
} insn_masks_t;

static insn_masks_t * get_insn_masks(insn_t insn, insn_kind_t kind, insn_masks_t * ri) {

    ri->rd = 0;
    ri->rn = 0;
    ri->rm = 0;
    ri->rt = 0;
    ri->cond = 0;
    
    switch (kind) {
        case INSN_KIND_ARM:
            ri->cond = bit_mask(28, 31);
            if (ri->cond == 0xf) {
                /* instruction is unconditional */
                ri->cond = 0;
            }
            ri->rd = bit_mask(12, 15);
            ri->rt = bit_mask(12, 15);
            ri->rn = bit_mask(16, 19);
            ri->rm = bit_mask(0, 3);
            break;
        case INSN_KIND_THUMB2:
            if (match(insn, "11110-----------10-0------------"))
                ri->cond = bit_mask(22, 25);
            else
                ri->cond = 0;
            ri->rd = bit_mask(8, 11);
            ri->rt = bit_mask(12, 15);
            ri->rn = bit_mask(16, 19);
            ri->rm = bit_mask(0, 3);
            break;
        case INSN_KIND_THUMB: {
                insn_t opcode = bits(insn, 11, 15);
                
                if (match(opcode, "1101-")) {
                    /* conditional branch */
                    ri->cond = bit_mask(8, 11);
                    ri->rd = 0;
                    ri->rn = 0;
                    ri->rm = 0;
                    ri->rt = 0;
                } else if (match(insn, "1011-0-1--------")) {
                    /* cbz/cbnz */
                    ri->cond = bit_mask(11, 11);
                    ri->rn = bit_mask(0, 2);
                } else if (
                    match(opcode, "1001-") ||
                    match(opcode, "1010-") ||
                    match(opcode, "1100-") ||
                    match(opcode, "01001") ||
                    match(opcode, "001--")) {
                    ri->rd = bit_mask(8, 10);
                    ri->rn = bit_mask(8, 10);
                    ri->rt = bit_mask(8, 10);
                } else if (
                    match(opcode, "00011") ||
                    match(opcode, "1000-") ||
                    match(opcode, "0101-") ||
                    match(opcode, "011--")) {
                    ri->rd = bit_mask(0, 2);
                    ri->rn = bit_mask(3, 5);
                    ri->rm = bit_mask(6, 8);
                    ri->rt = bit_mask(0, 2);
                } else if (
                    match(opcode, "000--") ||
                    match(opcode, "01000")) {
                    ri->rd = bit_mask(0, 2);
                    ri->rn = bit_mask(0, 2);
                    ri->rm = bit_mask(3, 5);
                }
            }
            
    }
    
    return ri;
}

static insn_t patch_by_mask(insn_t src, insn_t dst, insn_t mask1, insn_t mask2) {

    assert(bits_count(mask1) == bits_count(mask2));
    
    insn_t val = get_bits_by_mask(src, mask1);
    return (dst & ~mask2) | set_bits_by_mask(val, mask2);
    
}


static insn_t select_temporary_reg(insn_t insn, const insn_masks_t * masks) {

    insn_t rd = masks->rd ? get_bits_by_mask(insn, masks->rd) : 0xf;
    insn_t rt = masks->rd ? get_bits_by_mask(insn, masks->rt) : 0xf;
    insn_t rn = masks->rd ? get_bits_by_mask(insn, masks->rn) : 0xf;
    insn_t rm = masks->rd ? get_bits_by_mask(insn, masks->rm) : 0xf;
    
    insn_t rc;
    
    for (rc = 0; rc < 8; ++rc) {
        if ((rc != rd) && (rc != rt) && (rc != rn) && (rc != rm))
            return rc;
    }
    
    adbi_bug_unrechable();
    
}

static void patch_insn(const template_field_t * field, template_instance_t * instance,
                       const template_values_t * values) {
                       
    insn_masks_t src, dst;
    
    insn_kind_t dst_kind;
    insn_t dst_insn;
    
    insn_t mask;
    
    switch (values->insn_kind) {
        case INSN_KIND_ARM:
            dst_insn = * ((insn_t *)(instance->data + field->offset));
            dst_kind = INSN_KIND_ARM;
            break;
        case INSN_KIND_THUMB:
        case INSN_KIND_THUMB2:
            dst_insn = * ((uint16_t *)(instance->data + field->offset));
            dst_kind = INSN_KIND_THUMB;
            if (dst_insn >= 0xe800) {
                dst_insn = * ((insn_t *)(instance->data + field->offset));
                dst_insn = thumb2_swap_halfwords(dst_insn);
                dst_kind = INSN_KIND_THUMB2;
            }
            break;
        default:
            assert(0);
            break;
    }
    
    get_insn_masks(values->insn, values->insn_kind, &src);
    get_insn_masks(dst_insn, dst_kind, &dst);
    
#define MC(n, s, d)                                                         \
case TF_PATCH_ ## n:                                                    \
    mask = dst.d;                                                       \
    dst_insn = patch_by_mask(values->insn, dst_insn, src.s, dst.d);     \
    break;
    
    switch (field->field) {
            MC(COND, cond, cond);
            
            MC(RD2RD, rd, rd);
            MC(RT2RD, rd, rt);
            MC(RN2RD, rd, rn);
            MC(RM2RD, rd, rm);
            
            MC(RD2RT, rt, rd);
            MC(RT2RT, rt, rt);
            MC(RN2RT, rt, rn);
            MC(RM2RT, rt, rm);
            
            MC(RD2RN, rn, rd);
            MC(RT2RN, rn, rt);
            MC(RN2RN, rn, rn);
            MC(RM2RN, rn, rm);
            
            MC(RD2RM, rm, rd);
            MC(RT2RM, rm, rt);
            MC(RN2RM, rm, rn);
            MC(RM2RM, rm, rm);
            
        case TF_PATCH_RD2RS:
            mask = dst.rd;
            dst_insn = patch_by_mask(select_temporary_reg(values->insn, &dst), dst_insn, bit_mask(0,
                                     bits_count(mask) - 1), mask);
            break;
        case TF_PATCH_RT2RS:
            mask = dst.rt;
            dst_insn = patch_by_mask(select_temporary_reg(values->insn, &dst), dst_insn, bit_mask(0,
                                     bits_count(mask) - 1), mask);
            break;
        case TF_PATCH_RM2RS:
            mask = dst.rm;
            dst_insn = patch_by_mask(select_temporary_reg(values->insn, &dst), dst_insn, bit_mask(0,
                                     bits_count(mask) - 1), mask);
            break;
        case TF_PATCH_RN2RS:
            mask = dst.rn;
            dst_insn = patch_by_mask(select_temporary_reg(values->insn, &dst), dst_insn, bit_mask(0,
                                     bits_count(mask) - 1), mask);
            break;
            
        default:
            assert(0);
            break;
    }
    
#undef MC
    
    switch (dst_kind) {
        case INSN_KIND_THUMB:
            template_insert_u16_bits(instance, field->offset, dst_insn, mask);
            return;
        case INSN_KIND_THUMB2:
            dst_insn = thumb2_swap_halfwords(dst_insn);
            mask = thumb2_swap_halfwords(mask);
            template_insert_u32_bits(instance, field->offset, dst_insn, mask);
            return;
        case INSN_KIND_ARM:
            template_insert_u32_bits(instance, field->offset, dst_insn, mask);
            return;
        default:
            assert(0);
    }
    
}


static insn_t thumb2_branch_decode(insn_t insn) {
    insn_t imm10 = bits(insn, 16 + 0, 16 + 9);
    insn_t imm11 = bits(insn, 0, 10);
    insn_t s = bit(insn, 16 + 10);
    insn_t j1 = bit(insn, 13);
    insn_t j2 = bit(insn, 11);
    insn_t i1 = !(s ^ j1);
    insn_t i2 = !(s ^ j2);
    
    insn_t imm32 = 0;
    imm32 |= imm11 << 1;
    imm32 |= imm10 << 12;
    imm32 |= i2 << 22;
    imm32 |= i1 << 23;
    imm32 |= s << 24;
    imm32 = sign_extend(imm32, 24);
    
    return imm32;
}

#define imatch(p) match(insn, p)

static template_instance_t * template_fillout(const template_t * template, const template_values_t * values) {

    template_instance_t * instance = template_instatiate(template);
    
    template_field_t * field;
    
    insn_t insn = values->insn;
    
    for (field = template->fields; field->field; ++field) {
    
        switch (field->field) {
        
            case TF_HANDLER_ADDRESS:
                template_insert_u32(instance, field->offset, values->handler_address);
                break;
                
            case TF_ORIG_PC: {
                    uint32_t val = values->pc;
                    if (values->insn_kind != INSN_KIND_ARM)
                        val |= 0x01;
                    template_insert_u32(instance, field->offset, val);
                    break;
                }
                
            case TF_READ_PC: {
                    uint32_t val = values->pc;
                    if (values->insn_kind == INSN_KIND_ARM)
                        val += 8;
                    else
                        val += 4;
                    template_insert_u32(instance, field->offset, val);
                    break;
                }
                
            case TF_NEXT_PC: {
                    uint32_t val = values->next_pc;
                    if (values->insn_kind != INSN_KIND_ARM)
                        val |= 0x01;
                    template_insert_u32(instance, field->offset, val);
                    break;
                }
                
            case TF_INSN: {
                    if (values->insn_kind == INSN_KIND_THUMB) {
                        assert((values->insn >> 16) == 0);
                        template_insert_u16(instance, field->offset, (uint16_t) values->insn);
                    } else {
                        insn_t insn;
                        if (values->insn_kind != INSN_KIND_ARM) {
                            insn = thumb2_swap_halfwords(values->insn);
                        } else {
                            insn = values->insn;
                        }
                        template_insert_u32(instance, field->offset, insn);
                    }
                    break;
                }
                
            case TF_CBZ_T1_TARGET: {
                    assert(match(values->insn, "1011-0-1--------"));
                    insn_t imm5 = (values->insn >> 3) & 0x1f;
                    insn_t i = (values->insn >> 9) & 0x1;
                    insn_t imm32 = ((i << 5) | imm5) << 1;
                    address_t result = values->pc + 4 + imm32;
                    result |= 0x1;
                    template_insert_u32(instance, field->offset, result);
                    break;
                }
                
            case TF_ADR_T1_VAL: {
                    assert(imatch("10100-----------"));
                    insn_t imm8 = values->insn & 0xff;
                    address_t addr = values->pc + 4;
                    addr &= 0xfffffffc;     /* align */
                    addr += (imm8 << 2);
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_ADR_T2_VAL:
            case TF_ADR_T3_VAL: {
                    int t2 = imatch("11110-10101011110---------------");
                    int t3 = imatch("11110-10000011110---------------");
                    
                    assert(t2 || t3);
                    
                    insn_t imm8 = bits(values->insn, 0, 7);
                    insn_t imm3 = bits(values->insn, 12, 14);
                    insn_t i = bit(values->insn, 16 + 10);
                    insn_t imm32 = (i << 11) | (imm3 << 8) | imm8;
                    
                    address_t addr = values->pc + 4;
                    addr &= 0xfffffffc;     /* align */
                    
                    if (t2)
                        addr -= imm32;
                    else
                        addr += imm32;
                        
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_LDR_LIT_T1_ADDRESS: {
                    assert(match(values->insn, "01001-----------"));
                    address_t addr = (values->pc + 4);
                    address_t imm8 = values->insn & 0xff;
                    addr &= 0xfffffffc;     /* discard last 2 bits */
                    addr += imm8 * 4;       /* apply offset */
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_LDR_LIT_T2_ADDRESS: {
                    assert(match(values->insn, "11111000-1011111----------------"));
                    address_t addr = (values->pc + 4);
                    address_t imm12 = bits(values->insn, 0, 11);
                    address_t imm32 = imm12;
                    addr &= 0xfffffffc;     /* discard last 2 bits */
                    
                    /* apply offset */
                    if (bit(values->insn, 16 + 7)) {
                        addr += imm32;
                    } else {
                        addr -= imm32;
                    }
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_B_T1_TARGET: {
                    assert(imatch("1101------------"));
                    assert(!imatch("1101111---------"));   /* svc or undefined */
                    
                    address_t addr = (values->pc + 4);
                    address_t imm8 = bits(values->insn, 0, 7);
                    address_t imm32 = sign_extend(imm8 << 1, 9);
                    addr += imm32;
                    addr |= 0x1;    /* jump to Thumb */
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_B_T2_TARGET: {
                    assert(imatch("11100-----------"));
                    
                    address_t addr = (values->pc + 4);
                    address_t imm11 = bits(values->insn, 0, 10);
                    address_t imm32 = sign_extend(imm11 << 1, 12);
                    addr += imm32;
                    addr |= 0x1;    /* jump to Thumb */
                    template_insert_u32(instance, field->offset, addr);
                    break;
                }
                
            case TF_B_T3_TARGET: {
                    assert(imatch("11110-----------10-0------------"));
                    insn_t imm11 = bits(values->insn, 0, 10);
                    insn_t s = bit(values->insn, 16 + 10);
                    insn_t j1 = bit(values->insn, 13);
                    insn_t j2 = bit(values->insn, 11);
                    
                    insn_t imm32 = sign_extend((imm11 | j1 << 11 | j2 << 12 | s << 13) << 1, 14);
                    
                    address_t target = (values->pc + 4) + imm32;
                    target |= 0x1;
                    
                    template_insert_u32(instance, field->offset, target);
                    break;
                }
                
            case TF_B_T4_TARGET:
            case TF_BL_T1_TARGET: {
                    insn_t imm32 = thumb2_branch_decode(values->insn);
                    address_t target = (values->pc + 4) + imm32;
                    target |= 0x1;
                    template_insert_u32(instance, field->offset, target);
                    break;
                }
                
            case TF_BLX_T2_TARGET: {
                    /* The BL and BLX Thumb instructions use almost the same lovely encoding. */
                    assert(imatch("11110-----------11--------------"));
                    
                    insn_t imm32 = thumb2_branch_decode(values->insn);
                    
                    /* Note: pc value is aligned before adding the offset */
                    address_t target = (values->pc + 4) & 0xfffffffc;
                    target += imm32;
                    assert((values->insn & 1) == 0);
                    assert((target & 3) == 0);
                    template_insert_u32(instance, field->offset, target);
                    break;
                }
                
            case TF_BL_IMM_A1_TARGET:
            case TF_B_A1_TARGET: {
                    address_t imm24 = values->insn & 0x00ffffff;
                    address_t imm32 = sign_extend(imm24 << 2, 26);
                    address_t target = (values->pc + 8) + imm32;
                    template_insert_u32(instance, field->offset, target);
                    break;
                }
                
            case TF_BLX_IMM_A2_TARGET: {
                    address_t imm24 = (values->insn & 0x00ffffff);
                    address_t h = (values->insn & (1 << 24));
                    address_t imm32 = sign_extend((imm24 << 2) | (h ? 0x2 : 0x0), 26);
                    address_t target = (values->pc + 8) + imm32;
                    template_insert_u32(instance, field->offset, target | 0x1);
                    break;
                }
                
            /*** Instruction patching ***/
            case TF_PATCH_COND:
            case TF_PATCH_RD2RD:
            case TF_PATCH_RD2RT:
            case TF_PATCH_RD2RM:
            case TF_PATCH_RD2RN:
            case TF_PATCH_RT2RD:
            case TF_PATCH_RT2RT:
            case TF_PATCH_RT2RM:
            case TF_PATCH_RT2RN:
            case TF_PATCH_RM2RD:
            case TF_PATCH_RM2RT:
            case TF_PATCH_RM2RM:
            case TF_PATCH_RM2RN:
            case TF_PATCH_RN2RD:
            case TF_PATCH_RN2RT:
            case TF_PATCH_RN2RM:
            case TF_PATCH_RN2RN:
            case TF_PATCH_RD2RS:
            case TF_PATCH_RT2RS:
            case TF_PATCH_RM2RS:
            case TF_PATCH_RN2RS:
                patch_insn(field, instance, values);
                break;
                
            case TF_NULL:
                return instance;
        }
        
    }
    
    return instance;
}

template_instance_t * template_get_handler(
    const template_t * template,
    address_t trampoline_address __attribute__((unused)),
    address_t insn_address,
    address_t handler_address,
    insn_t insn,
    insn_kind_t insn_kind) {
    
    template_values_t values;
    
    values.pc = insn_address;
    values.insn = insn;
    values.handler_address = handler_address;
    values.insn_kind = insn_kind;
    if (insn_kind == INSN_KIND_THUMB)
        values.next_pc = values.pc + 2;
    else
        values.next_pc = values.pc + 4;
        
    return template_fillout(template, &values);
}

bool template_need_return_jump(const template_t * template __attribute__((unused))) {
    return false;
}

void template_iter_return_address(
        address_t pc __attribute__((unused)),
        insn_t insn __attribute__((unused)),
        insn_kind_t kind __attribute__((unused)),
        const template_t * template __attribute__((unused)),
        address_t trampoline_addr __attribute__((unused)),
        template_return_callback_t callback __attribute__((unused))) {}

insn_kind_t template_get_template_kind(const template_t * template __attribute__((unused))) { return INSN_KIND_ARM; }

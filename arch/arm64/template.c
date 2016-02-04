#include "tracepoint/template.h"
#include "match.h"

#include "util/bitops.h"

typedef struct template_values_t {
    address_t handler_address;
    address_t trampoline_address;
    address_t pc;
    address_t next_pc;
    insn_t insn;
    insn_kind_t insn_kind;
} template_values_t;

/*static void return_trampoline_a64(const template_field_t * field, template_instance_t * instance,
                       const template_values_t * values) {
    assert(values->insn_kind == INSN_KIND_A64);

    address_t return_place = values->trampoline_address + field->offset;
    address_t return_address;

    switch (field->field) {
    case TF_HANDLER_RETURN:
        return_address = values->next_pc;
        break;
    case TF_HANDLER_RETURN_TO_IMM26:
        break;
    case TF_HANDLER_RETURN_TO_IMM19:
        break;
    case TF_HANDLER_RETURN_TO_IMM14:
        break;
    default:
        break;
    }
}*/

static void patch_insn_a64(const template_field_t * field, template_instance_t * instance,
                       const template_values_t * values) {
    assert(values->insn_kind == INSN_KIND_A64);

    insn_t dst_insn = *((insn_t *) (instance->data + field->offset));
    insn_t src_insn = values->insn;

    insn_t mask;

    switch (field->field) {
    case TF_PATCH_COND: /* b.cond */
        assert(match(values->insn, "01010100-------------------0----"));
        mask = 0xf;
        break;
    case TF_PATCH_SF2SF:
        mask = 0x80000000;
        break;
    case TF_PATCH_OP2OP:
        mask = 0x01000000;
        break;
    case TF_PATCH_RN2RN:
        mask = 0x000003e0;
        break;
    case TF_PATCH_RT2RT:
        mask = 0x0000001f;
        break;
    case TF_PATCH_RN2RT:
        mask = 0x000003e0;
        src_insn <<= 5;
        break;
    case TF_PATCH_B40_2_B40:    /* tbz, tbnz */
        assert(match(values->insn, "-011011-------------------------"));
        mask = 0x80f80000;
        break;
    case TF_PATCH_LDR_SIZE_LIT2REG:
        mask = 0x40000000;
        break;
    default:
        assert(0);
        break;
    }

    dst_insn = (dst_insn & ~mask) | (src_insn & mask);
    template_insert_u32(instance, field->offset, dst_insn);
}

static template_instance_t * template_fillout(const template_t * template, const template_values_t * values) {

    template_instance_t * instance = template_instatiate(template);
    template_field_t * field;
    insn_t insn = values->insn;
    insn_kind_t kind = values->insn_kind;

   for (field = template->fields; field->field; ++field) {
       switch (field->field) {
       case TF_HANDLER_ADDRESS:
           if (kind == INSN_KIND_A64)
               template_insert_u64(instance, field->offset, values->handler_address);
           else
               template_insert_u32(instance, field->offset, values->handler_address);
           break;

/*       case TF_ORIG_PC: {
               uint64_t val = values->pc;
               if ((kind == INSN_KIND_T32_32) || (kind == INSN_KIND_T32_16))
                   val |= 0x01;

               if (kind == INSN_KIND_A64)
                   template_insert_u64(instance, field->offset, val);
               else
                   template_insert_u32(instance, field->offset, val);
               break;
           }*/

       case TF_NEXT_PC: {
               uint64_t val = values->next_pc;
               if ((kind == INSN_KIND_T32_32) || (kind == INSN_KIND_T32_16))
                   val |= 0x01;

               if (kind == INSN_KIND_A64)
                   template_insert_u64(instance, field->offset, val);
               else
                   template_insert_u32(instance, field->offset, val);
               break;
           }

       case TF_INSN: {
               switch (kind) {
               case INSN_KIND_T32_16:
                   assert((values->insn >> 16) == 0);
                   template_insert_u16(instance, field->offset, (uint16_t) values->insn);
                   break;
               case INSN_KIND_T32_32:
                   insn = t32_swap_halfwords(values->insn);
               case INSN_KIND_A32:
                   template_insert_u32(instance, field->offset, (uint32_t) insn);
                   break;
               case INSN_KIND_A64:
                   template_insert_u32(instance, field->offset, insn);
                   break;
               }
               break;
           }

       case TF_ADRP_RESULT: {
           address_t dst;
           address_t base = values->pc;
           int op = !!(insn & 0x80000000);
           signed long imm = ((insn & 0x00ffffe0) >> 3) | ((insn & 0x60000000) >> 29);
           int neg = !!(imm & 0x100000);
           if (neg)
               imm |= 0xfffffffffff00000;
           if (op) {
               imm <<= 12;
               base &= ~((1<<12) - 1);
           }
           dst = base + imm;
           template_insert_u64(instance, field->offset, dst);
           break;
       }
       case TF_RELATIVE_IMM19_ADDR: {
           address_t dst = values->pc;
           unsigned long imm19 = (insn & 0x00ffffe0) >> 3;
           if (imm19 & 0x00100000) {
               imm19 |= 0xfffffffffff00000;
               imm19 = (~imm19) + 1;
               dst -= imm19;
           } else {
               dst += imm19;
           }
           template_insert_u64(instance, field->offset, dst);
           break;
       }
       case TF_RELATIVE_IMM26_ADDR: {
           address_t dst = values->pc;
           unsigned long imm26 = (insn & 0x03ffffff) << 2;
           if (imm26 & 0x08000000) {
               imm26 |= 0xfffffffff8000000;
               imm26 = (~imm26) + 1;
               dst -= imm26;
           } else {
               dst += imm26;
           }
           template_insert_u64(instance, field->offset, dst);
           break;
       }
       case TF_PATCH_COND:
       case TF_PATCH_SF2SF:
       case TF_PATCH_OP2OP:
       case TF_PATCH_RN2RN:
       case TF_PATCH_RT2RT:
       case TF_PATCH_RN2RT:
       case TF_PATCH_B40_2_B40:
       case TF_PATCH_LDR_SIZE_LIT2REG:
           if (kind == INSN_KIND_A64)
               patch_insn_a64(field, instance, values);
           break;

       case TF_HANDLER_RETURN:
       case TF_HANDLER_RETURN_TO_IMM26:
       case TF_HANDLER_RETURN_TO_IMM19:
       case TF_HANDLER_RETURN_TO_IMM14:
           //return_trampoline_a64(field, instance, values);
           break;

       case TF_NULL:
           return instance;
       }
   }
    return instance;
}

template_instance_t * template_get_handler(
        const template_t * template,
        address_t trampoline_address,
        address_t insn_address,
        address_t handler_address,
        insn_t insn,
        insn_kind_t insn_kind) {
    
    template_values_t values;

    values.pc = insn_address;
    values.insn = insn;
    values.handler_address = handler_address;
    values.trampoline_address = trampoline_address;
    values.insn_kind = insn_kind;
    if (insn_kind == INSN_KIND_T32_16)
        values.next_pc = values.pc + 2;
    else
        values.next_pc = values.pc + 4;

    return template_fillout(template, &values);
}
bool template_need_return_jump(const template_t * template) {
    template_field_t * field;
    for (field = template->fields; field->field; ++field) {
        if ((field->field == TF_HANDLER_RETURN) ||
                (field->field == TF_HANDLER_RETURN_TO_IMM26) ||
                (field->field == TF_HANDLER_RETURN_TO_IMM19) ||
                (field->field == TF_HANDLER_RETURN_TO_IMM14))
            return true;
    }
    return false;
}

void template_iter_return_address(address_t pc, insn_t insn, insn_kind_t kind, const template_t * template,
        address_t trampoline_addr, template_return_callback_t callback) {
    template_field_t * field;
    if (kind != INSN_KIND_A64)
        return;

    for (field = template->fields; field->field; ++field) {
        address_t dst = pc;
        switch (field->field) {
        case TF_HANDLER_RETURN:
            callback(trampoline_addr + field->offset, dst + 4);
            break;
        case TF_HANDLER_RETURN_TO_IMM26:
            {
                address_t imm = (insn & 0x03ffffff) << 2;
                if (imm & (0x08000000)) {
                    imm |= 0xfffffffff8000000;
                    imm = (~imm) + 1;
                    dst -= imm;
                } else {
                    dst += imm;
                }
                callback(trampoline_addr + field->offset, dst);
                break;
            }
        case TF_HANDLER_RETURN_TO_IMM19:
            {
                address_t imm = ((insn & 0x00ffffe0) >> 3);
                if (imm & 0x00100000) {
                    imm |= 0xfffffffffff00000;
                    imm = (~imm) + 1;
                    dst -= imm;
                } else {
                    dst += imm;
                }
                callback(trampoline_addr + field->offset, dst);
                break;
                }
        case TF_HANDLER_RETURN_TO_IMM14:
            {
                address_t imm = ((insn & 0x0007ffe0) >> 3);
                if (imm & 0x00008000) {
                    imm |= 0xffffffffffff8000;
                    imm = (~imm) + 1;
                    dst -= imm;
                } else {
                    dst += imm;
                }
                callback(trampoline_addr + field->offset, dst);
                break;
            }
        default:
            break;
        }
    }
}

insn_kind_t template_get_template_kind(const template_t * template) {
    return strncmp("a64", template->name, 3) ? INSN_KIND_A32 : INSN_KIND_A64;
}

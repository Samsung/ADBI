#include "tracepoint/template.h"

const template_t * template_select_arm(insn_t insn);
const template_t * template_select_thumb(insn_t insn);
const template_t * template_select_thumb32(insn_t insn);

const template_t * template_select(insn_t insn, insn_kind_t kind) {
    switch (kind) {
        case INSN_KIND_THUMB:
            return template_select_thumb(insn);
        case INSN_KIND_THUMB2:
            return template_select_thumb32(insn);
        case INSN_KIND_ARM:
            return template_select_arm(insn);
        default:
            return NULL;
    }
}

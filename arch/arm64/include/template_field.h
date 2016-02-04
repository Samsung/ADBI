/*
 * template_field.h
 */

#ifndef ARCH_ARM64_INCLUDE_TEMPLATE_FIELD_H_
#define ARCH_ARM64_INCLUDE_TEMPLATE_FIELD_H_

typedef enum template_field_id_t {

    /* End marker */
    TF_NULL = 0,

    /* Insert the original traced instruction opcode */
    TF_INSN,

    /* Absolute address of the high-level handler */
    TF_HANDLER_ADDRESS,

    /* Insert the address of the traced instruction. */
    //TF_ORIG_PC,

    /* Insert the address of the instruction following the traced instruction */
    TF_NEXT_PC,

    /* Calculate address pc + imm19 */
    TF_RELATIVE_IMM19_ADDR,

    /* Calculate address pc + imm26 */
    TF_RELATIVE_IMM26_ADDR,

    /*** Instruction patching ***/
    /* Copy condition of b instruction */
    TF_PATCH_COND,

    /* Copy sf bit */
    TF_PATCH_SF2SF,

    /* Copy op bit */
    TF_PATCH_OP2OP,

    /* Patch Rn to original instruction Rn */
    TF_PATCH_RN2RN,
    /* Patch Rt to original instruction Rt */
    TF_PATCH_RT2RT,
    /* Patch Rn to original instruction Rt */
    TF_PATCH_RN2RT,

    TF_PATCH_B40_2_B40,

    TF_PATCH_LDR_SIZE_LIT2REG,

    TF_ADRP_RESULT,

    /*** Return from trampoline ***/
    /* Return to the address of the instruction following the traced instruction */
    TF_HANDLER_RETURN,

    /* Return to the address PC + imm26 of the traced instruction */
    TF_HANDLER_RETURN_TO_IMM26,

    /* Return to the address PC + imm19 of the traced instruction */
    TF_HANDLER_RETURN_TO_IMM19,

    /* Return to the address PC + imm14 of the traced instruction */
    TF_HANDLER_RETURN_TO_IMM14,

} template_field_id_t;

#endif /* ARCH_ARM64_INCLUDE_TEMPLATE_FIELD_H_ */

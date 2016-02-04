#ifndef TEMPLATE_FIELD_H_
#define TEMPLATE_FIELD_H_

typedef enum template_field_id_t {

    /* End marker */
    TF_NULL = 0,
    
    /* Insert the original traced instruction opcode */
    TF_INSN,
    
    /* Absolute address of the high-level handler */
    TF_HANDLER_ADDRESS,
    
    /* Insert the address of the traced instruction. */
    TF_ORIG_PC,
    
    /* Insert the value of the PC at the traced instruction as seen by the processor */
    TF_READ_PC,
    
    /* Insert the address of the instruction following the traced instruction */
    TF_NEXT_PC,
    
    
    /*** Instruction patching ***/
    TF_PATCH_COND,
    
    TF_PATCH_RD2RD,
    TF_PATCH_RD2RT,
    TF_PATCH_RD2RM,
    TF_PATCH_RD2RN,
    
    TF_PATCH_RT2RD,
    TF_PATCH_RT2RT,
    TF_PATCH_RT2RM,
    TF_PATCH_RT2RN,
    
    TF_PATCH_RM2RD,
    TF_PATCH_RM2RT,
    TF_PATCH_RM2RM,
    TF_PATCH_RM2RN,
    
    TF_PATCH_RN2RD,
    TF_PATCH_RN2RT,
    TF_PATCH_RN2RM,
    TF_PATCH_RN2RN,
    
    TF_PATCH_RD2RS,
    TF_PATCH_RT2RS,
    TF_PATCH_RM2RS,
    TF_PATCH_RN2RS,
    
    /*** Helper values ***/
    
    /* Insert the result of the Thumb ADR instruction. */
    TF_ADR_T1_VAL,
    TF_ADR_T2_VAL,
    TF_ADR_T3_VAL,
    
    /* Insert load address of the Thumb load literal instruction (ldr3). */
    TF_LDR_LIT_T1_ADDRESS,
    TF_LDR_LIT_T2_ADDRESS,
    
    /* Insert branch addresses. */
    TF_B_T1_TARGET,
    TF_B_T2_TARGET,
    TF_CBZ_T1_TARGET,
    TF_B_T3_TARGET,
    TF_B_T4_TARGET,
    TF_BL_T1_TARGET,
    TF_BLX_T2_TARGET,
    TF_B_A1_TARGET,
    TF_BL_IMM_A1_TARGET,
    TF_BLX_IMM_A2_TARGET,
    
} template_field_id_t;

#define TF_PATCH_RD     TF_PATCH_RD2RD
#define TF_PATCH_RT     TF_PATCH_RT2RT
#define TF_PATCH_RN     TF_PATCH_RN2RN
#define TF_PATCH_RM     TF_PATCH_RM2RM

#endif

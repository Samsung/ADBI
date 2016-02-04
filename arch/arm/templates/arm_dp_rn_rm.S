/* OOL handler for the ARM data processing instructions, where:
 *      rn == rm == pc
 *      rd != pc
 *
 *   Opcodes:
 *       cccc 00xo ooos nnnn  dddd ssss ttt0 mmmm    register, immediate shift
 */

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond:
tf_patch_rd:
    ldr r1, tf_read_pc          /* load original pc value into rd */

tf_insn:
tf_patch_rn2rd:
tf_patch_rm2rd:
    add r0, r0, r1              /* original instruction with rn and rm changed to rd */

    ldr pc, tf_next_pc          /* unconditional return */

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_read_pc:
    .word BAD

HANDLER_END

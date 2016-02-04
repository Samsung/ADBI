/* OOL handler for the ARM data processing instructions, where:
 *      rn == pc
 *      rm != pc
 *      rd != pc
 *      rm != rd
 *
 *   Opcodes:
 *       cccc 00xo ooos nnnn  dddd rrrr iiii iiii    immediate
 *       cccc 00xo ooos nnnn  dddd ssss ttt0 mmmm    register, immediate shift
 *
 * This handler will also work for ldr and ldrb instructions (literal), opcodes:
 *       cccc 010x u1x1 1111  tttt iiii iiii iiii    ldrb (literal)
 *       cccc 010x u0x1 1111  tttt iiii iiii iiii    ldr (literal)
 */

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond:
tf_patch_rd:
    ldr r0, tf_read_pc          /* load original pc value into rd */
tf_insn:
tf_patch_rn2rd:
    add r0, r0, #0              /* original instruction with rn changed to rd */

    ldr pc, tf_next_pc          /* unconditional return */

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_read_pc:
    .word BAD

HANDLER_END

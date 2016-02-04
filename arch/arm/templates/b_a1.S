/* OOL handler for the ARM branch instruction with an immediate operand.
 *
 *   Opcode:
 *       cccc 1010 iiii iiii  iiii iiii iiii iiii    B   (immediate)
 */

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond:
    /* The following instruction will have the condition code of the original instruction assigned. If the
     * condition is true, it will cause a branch to the destination address. */
    ldr pc, tf_b_a1_target
    /* This instruction will be executed unconditionally, it will branch to the instruction following the
     * original instruction. Note that it will only be executed when the condition of the first instruction
     * is false. */
    ldr pc, tf_next_pc

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_b_a1_target:
    .word BAD

HANDLER_END

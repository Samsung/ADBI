/* OOL handler for the ARM branch and link with exchange instruction with an immediate operand.
 *
 *   Opcode:
 *       1111 101h iiii iiii  iiii iiii iiii iiii    BLX (immediate)
 */

#include "arm_handler.h"
HANDLER_BEGIN
    /* The following 2 instructions will be executed unconditionally. Note that loading a value to the pc can
     * cause an exchange only on ARMv5 and above.  */
    ldr lr, tf_next_pc
    ldr pc, tf_blx_imm_a2_target

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_blx_imm_a2_target:
    .word BAD        /* this value MUST have the last bit set to make it exchange */

HANDLER_END

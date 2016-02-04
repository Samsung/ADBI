import random

from test import *
from branch import *

def test(rm, fn):
    name = 'test_blx_reg_a1_%s' % tn()

    cleanup = asm_wrap(name, 'r0')
    print '    adr %s, %s' % (rm, fn)
    if fn.startswith('thumb'):
        print '    orr %s, #1' % (rm)
    print '%s_tinsn:' % name
    print '    blx %s' % (rm)
    cleanup()

def iter_cases():
    fun = 'thumb_fun_b thumb_fun_f arm_fun_b arm_fun_f'.split()
    while True:
        yield random.choice(T32REGS), random.choice(fun)
        
branch_helpers('b')
print '    .arm'
tests(test, iter_cases(), 30)
branch_helpers('f')
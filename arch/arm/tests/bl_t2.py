import random

from test import *
from branch import *

def test(direction):
    name = 'test_blx_t2_%s' % tn()
    
    cleanup = asm_wrap(name, 'r0')
    print '    mov r0, #0'
    print '%s_tinsn:' % name
    print '    blx.w arm_fun_%s' % (direction)
    cleanup()

def iter_cases():
    yield 'f'
    yield 'b'
        
branch_helpers('b')
print '    .thumb'
tests(test, iter_cases(), 30)
branch_helpers('f')
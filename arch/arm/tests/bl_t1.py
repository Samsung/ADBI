import random

from test import *
from branch import *

def test(direction):
    name = 'test_bl_t1_%s' % tn()
    
    cleanup = asm_wrap(name, 'r0')
    print '%s_tinsn:' % name
    print '    bl.w thumb_fun_%s' % (direction)
    cleanup()

def iter_cases():
    yield 'f'
    yield 'b'
        
branch_helpers('b')
print '    .thumb'
tests(test, iter_cases(), 30)
branch_helpers('f')
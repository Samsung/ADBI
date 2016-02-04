import random

from test import *
from branch import *

def test(direction):
    name = 'test_b_t2_%s' % tn()
    cleanup = asm_wrap(name, 'r0') 
    
    set_lr('%s_out' % name, True)
    
    print '%s_tinsn:' % name
    print '    b.n thumb_fun_%s' % direction

    print '%s_out:' % name
    cleanup()

def iter_cases():
    yield 'f'
    yield 'b'

branch_helpers('b')
print '    .thumb'
tests(test, iter_cases(), 2)
branch_helpers('f')
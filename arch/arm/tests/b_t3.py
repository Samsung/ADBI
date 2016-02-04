import random

from test import *
from branch import *

def test(flags, cond, direction):
    name = 'test_b_t3_%s' % tn()
    
    cleanup = asm_wrap(name, 'r0', {'r0':0}, flags)

    set_lr('%s_out' % name, True)
    
    print '%s_tinsn:' % name
    print '    b%s.w thumb_fun_%s' % (cond, direction)
    print '%s_out:' % name
    cleanup()

def iter_cases():
    while True:
        yield random.randint(0, 0x1f), random.choice(COND), random.choice('fb')
        
branch_helpers('b')
print '    .thumb'
tests(test, iter_cases(), 5)
branch_helpers('f')
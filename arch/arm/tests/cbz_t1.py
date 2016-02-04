import random

from test import *
from branch import *

def test(n, rn, zero, imm6):
    name = 'test_cbz_t1_%s' % tn()
    
    cleanup = asm_wrap(name, 'r0')

    if zero:
        print '    mov %s, #0' % rn
    else:
        print '    mov %s, #1' % rn
        
    print '%s_tinsn:' % name
    print '    cb%sz.n %s, %s_target' % (n, rn, name)
    print '    mov.w r0, #0'
    print '    b.w %s_out' % name
    for x in xrange(imm6 - 4):
        print '    .inst.n 0xbf00'
    print '%s_target:' % name
    print '    mov r0, #1'
    print '%s_out:' % name
    cleanup()

def iter_cases():
    while True:
        yield random.choice(['n', '']), random.choice(LOWREGS), random.randint(0, 1), random.randint(0, 1 << 6 - 1)
        
print '    .thumb'
tests(test, iter_cases(), 30)

from test import *
import random

print '    .thumb'

from literal import rubbish

def test(rt, imm8):
    imm10 = imm8 << 2
    name = 'test_ldr_lit_t1_%s' % tn()
    cleanup = asm_wrap(name, rt) 
    print '%s_tinsn:' % name
    print '    ldr.n %s, [pc, #%i]' % (rt, imm10)
    cleanup()
    
def iter_cases():
    while True:
        yield random.choice(LOWREGS), random.randint(0, 0xff)

print '    .thumb'
banner('tests')
tests(test, iter_cases(), 30)
banner('random data')
rubbish()

from test import *
import random

print '    .thumb'

from literal import rubbish

def test(rt, imm12):
    name = 'test_ldr_lit_t2_%s' % tn()
    cleanup = asm_wrap(name, rt) 
    print '%s_tinsn:' % name
    print '    ldr.w %s, [pc, #%i]' % (rt, imm12)
    cleanup()
    
def iter_cases():
    while True:
        yield random.choice(T32REGS), random.randint(-(2 << 12 - 1), 2 << 12 - 1)

print '    .thumb'
banner('random data')
rubbish()
banner('tests')
tests(test, iter_cases(), 30)
banner('random data')
rubbish()

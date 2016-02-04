from test import *
import random

print '    .thumb'

def test(rd, imm12):
    name = 'test_adr_t2_%s' % tn()
    cleanup = asm_wrap(name, rd) 
    print '%s_tinsn:' % name
    print '    sub.w %s, pc, #%i' % (rd, imm12)
    cleanup()
       

def iter_cases():
    while True:
        yield random.choice(T32REGS), random.randint(0, 4095)

print '    .thumb'
tests(test, iter_cases(), 30)
from test import *
import random

def test(rd, imm8):
    imm10 = imm8 << 2
    name = 'test_adr_t1_%s' % tn()
    cleanup = asm_wrap(name, rd) 
    print '%s_tinsn:' % name
    print '    add.n %s, pc, #%i' % (rd, imm10)
    cleanup()
    
def iter_cases():
    while True:
        yield random.choice(LOWREGS), random.randint(0, 255)

print '    .thumb'
tests(test, iter_cases(), 30)
import random

from test import *
from branch import *

INSN = 'and eor sub rsb add adc sbc rsc tst teq cmp cmn orr mov bic mvn'.split()

NORN = 'mov mvn'.split()
NORD = 'cmp cmn tst teq'.split() 

def rotate(val, c):
    return ((val >> c) | (val << (32 - c))) & 0xffffffff

def test(insn, s, flags, rd, rn, rnval, imm8, rot):
    name = 'test_dp_imm_%s' % tn()
       
    cleanup = asm_wrap(name, rd, {rn:rnval}, flags)
    
    print '%s_tinsn:' % name
    
    if 1:
        if insn in NORN:
            print '    %s%s %s, #%i, %i' % (insn, s, rd, imm8, rot)
        elif insn in NORD:
            print '    %s %s, #%i, %i' % (insn, rn, imm8, rot)
        else:
            print '    %s%s %s, %s, #%i, %i' % (insn, s, rd, rn, imm8, rot)
    else:
        v = rotate(imm8, rot)
        if insn in NORN:
            print '    %s%s %s, #%i       // %x ror %i ' % (insn, s, rd, v, imm8, rot)
        elif insn in NORD:
            print '    %s %s, #%i         // %x ror %i ' % (insn, rn, v, imm8, rot)
        else:
            print '    %s%s %s, %s, #%i   // %x ror %i ' % (insn, s, rd, rn, v, imm8, rot)

    cleanup()

def iter_cases():
    while True:
        yield (random.choice(INSN), random.choice(['s', '']), 
               random.randint(0, 0x1f), random.choice(T32REGS),
               random.choice(ALLREGS), random.randint(0, 0xffffffff), 
               random.randint(0, 0xff), random.randint(0, 0xf) * 2)
        
print '    .arm'
tests(test, iter_cases(), 300)

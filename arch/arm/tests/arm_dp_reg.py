import random

from test import *
from branch import *

INSN = 'and eor sub rsb add adc sbc rsc tst teq cmp cmn orr mov bic mvn'.split()
NORN = 'mov mvn'.split()
NORD = 'cmp cmn tst teq'.split() 

def test(insn, s, flags, rd, rn, rnval, rm, rmval):
    name = 'test_dp_reg_%s' % tn()
        
    cleanup = asm_wrap(name, rd, {rn:rnval, rm:rmval}, flags)
    
    print '%s_tinsn:' % name
    
    if insn in NORN:
        print '    %s%s %s, %s' % (insn, s, rd, rm)
    elif insn in NORD:
        print '    %s %s, %s' % (insn, rn, rm)
    else:
        print '    %s%s %s, %s, %s' % (insn, s, rd, rn, rm)

    cleanup()

def iter_cases():
    while True:
        yield (random.choice(INSN), random.choice(['s', '']), 
               random.randint(0, 0x1f), random.choice(T32REGS),
               random.choice(ALLREGS), random.randint(0, 0xffffffff), 
               random.choice(ALLREGS), random.randint(0, 0xffffffff))
        
print '    .arm'
tests(test, iter_cases(), 300)

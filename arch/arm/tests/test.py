import random
random.seed(0xadb1)

print '    .syntax unified'
print '    .arch armv7-a'

print '    .text'
print 

SCRATCHREGS = 'r0 r1 r2 r3'.split()
LOWREGS = 'r0 r1 r2 r3 r4 r5 r6 r7'.split()
HIGHREGS = 'r8 r9 r10 r11 r12 sp lr pc'.split()
ALLREGS = 'r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 sp lr pc'.split()
T32REGS = 'r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 lr'.split()
NOPCREGS = 'r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 sp lr'.split()
COND = 'eq ne cs cc mi pl vs vc hi ls ge lt gt le'.split()

def banner(text):
    print '/*'.ljust(110, '*')
    print '    %s' % text.strip() 
    print '*/'.rjust(110, '*')

def regsortkey(val):
    order = LOWREGS + HIGHREGS
    return order.index(val)

def asm_wrap(name, rd=None, opregs=None, flags=None):
    def cleanup():
        print '    @ xor the result registers into r0'
        if 'r0' not in rd:
            print '    mov r0, #0'
        for reg in set(rd) - set(['r0']):
            print '    eor r0, %s' % reg

        print '    @ xor the cpsr'
        cpsr_tmp = [reg for reg in dirty | set(SCRATCHREGS) if reg not in [sp_copy, 'r0']][0]
        print '    mrs %s, CPSR' % cpsr_tmp
        print '    and %s, #0xf8000000' % cpsr_tmp
        print '    eor r0, %s' % cpsr_tmp

        if sp_copy:
            print '    @ Restore original stack pointer.'
            print '    mov sp, %s' % sp_copy
    
        print '    @ restore register values '
        print '    pop {%s}' % ', '.join(sorted(pop, key=regsortkey))
        print '    .ltorg'
        print '    .size %s, . - %s' % (name, name)
        print 

    print '    .global %s' % name
    print '    .type %s, %%function' % name    
    print '%s:' % name
    
    if not opregs:
        opregs = {}        
    
    if 'pc' in opregs:
        # Do not allow to modify the pc
        del opregs['pc']
        
    if 'sp' in opregs:
        # Do not allow to modify the sp
        del opregs['sp']
        
    if not rd:
        rd = set()
    if isinstance(rd, str):
        rd = [rd]
    rd = set(rd)
    
    dirty = set(rd) | set(opregs) 
    dirty.add('r0') # r0 is always dirty -- it contains the result

    if 'sp' in dirty:
        sp_copy = [reg for reg in T32REGS if reg not in dirty][0]
        dirty.add(sp_copy)
    else:
        sp_copy = ''

    print '    @ dirty registers:', ', '.join(sorted(dirty, key=regsortkey))
    push = dirty - set(SCRATCHREGS)
    push.discard('sp')
    pop = set(push)
    push.add('lr')
    pop.add('pc')
    pop.discard('lr')

    print '    @ store register values '
    print '    push {%s}' % ', '.join(sorted(push, key=regsortkey))

    if sp_copy:
        print '    @ the stack pointer will be overwritten, store it in an additional register'
        print '    mov %s, sp' % sp_copy
    
    if flags:
        def bin(val, width=0):
            return '{0:b}'.format(val).rjust(width, '0')        
        tmp = [reg for reg in dirty if reg != sp_copy][0]
        print '    @ initialize nzcvq flags to %s ' % bin(flags, 5) 
        print '    mov %s, #%s' % (tmp, hex(flags << 27))
        print '    msr CPSR_f, %s' % tmp
    
    autoinit = rd | set(['r0']) - set(opregs) - set(sp_copy)
    if autoinit:
        print '    @ initialize result registers'
        for reg in autoinit:
            print '    mov %s, #0' % reg
    
    if opregs:
        print '    @ initialize operand registers'
        for reg, val in opregs.items():
            print '    ldr %s, =0x%08x' % (reg, val)
    
    return cleanup
    
    
def genertate_test_cases(iterable, count):
    result = set()
    for case in iterable:
        result.add(case)
        if len(result) >= count:
            break
    return result

def tests(fn, iterable, count):
    cases = genertate_test_cases(iterable, count)
    for case in cases:
        fn(*case)
    
TESTNUM = 0
def tn():
    global TESTNUM
    TESTNUM += 1
    return '%03x' % TESTNUM

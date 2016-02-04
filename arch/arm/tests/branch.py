import random

def branch_helpers(dir):
    
    print '    .arm'       
    print 'arm_fun_%s:' % dir
    print '    ldr r0, =%i' % random.randint(1, 0xffffffff)
    print '    bx lr'
           
    print '    .thumb'
    print 'thumb_fun_%s:' % dir
    print '    ldr r0, =%i' % random.randint(1, 0xffffffff)
    print '    bx lr'
    
    print '    .ltorg'
    
def set_lr(symbol, thumb=False):
    print '    adr lr, %s   /* artificially set the lr */' % symbol
    if thumb:
        print '    orr lr, #1   /* make sure lr points to thumb Thumb code */'

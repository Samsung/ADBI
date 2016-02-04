import random

def rubbish():
    '''Generate 4KB of random data.'''
    PERLINE = 8

    for a in xrange(4096 / PERLINE):
        data = ['0x%08x' % random.randint(0, 0xffffffff) for x in xrange(PERLINE + 1)]
        print '    .word %s' % (', '.join(data))

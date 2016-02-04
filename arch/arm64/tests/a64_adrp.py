import random
from common import *

class test_a64_adrp(TemplateTest):
    
    def gen_rand(self):
        while True:
            yield {'insn' : random.choice(['adrp', 'adr']),
                   'res'  : random.choice(GPREGS64),
                   'label': random.choice(self.symbols)}

    symbols = [__name__ + '_addr_' + str(i) for i in xrange(16)]

    def test_begin(self):
        yield '    .arch armv8-a'
        yield '    .align 2'
        yield '    .text'
        for i in xrange(0, len(self.symbols), 3):
            yield self.symbols[i] + ':'
            yield '    nop'
            yield '    .skip %d' % random.randrange(1024, 3072, 4)
        yield '    .text'

    def gen_testcase(self, nr, insn, res, label):
        state = ProcessorState([res])
        yield state.prepare()
        yield self.testcase_insn(nr, '{insn}\t{res}, {label}'.format(**locals()))
        l = 'pageof:' + label if insn == 'adrp' else label
        yield state.check({res:l})
        yield state.restore()
        
    def test_end(self):
        yield '    .align 2'
        yield '    .text'
        for i in xrange(1, len(self.symbols), 3):
            yield self.symbols[i] + ':'
            yield '    nop'
            yield '    .skip %d' % random.randrange(1024, 3072, 4)
        yield '    .data'
        for i in xrange(2, len(self.symbols), 3):
            yield self.symbols[i] + ':'
            yield '    nop'
            yield '    .skip %d' % random.randrange(1025, 3071)

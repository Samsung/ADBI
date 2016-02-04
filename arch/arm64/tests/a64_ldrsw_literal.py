import random
from common import *

class test_a64_ldrsw_literal(TemplateTest):
    
    def gen_rand(self):
        while True:
            yield {'reg'  : random.choice(GPREGS64),
                   'label_idx': random.randint(0, self.__label_count - 1)}

    def __init__(self):
        self.__label_count = 12
        self.symbols = [__name__ + '_addr_' + str(i) for i in xrange(self.__label_count)]
        self.randvals = random.sample(xrange(0, 0xffffffff), self.__label_count)
    def test_begin(self):
        yield '    .arch armv8-a'
        yield '    .align 2'
        yield '    .text'
        for i in xrange(0, len(self.symbols), 2):
            yield self.symbols[i] + ':'
            yield '    .word %s' % hex(self.randvals[i])
            yield '    .skip %d' % random.randrange(512, 2048, 4)
        yield '    .text'

    def gen_testcase(self, nr, reg, label_idx):
        label = self.symbols[label_idx]
        val = self.randvals[label_idx]
        if val > 0x7fffffff:
            assert len(hex(val)) == 10
            val = '0xffffffff' + hex(val)[2:]
        state = ProcessorState(save=[reg])
        yield state.prepare()
        yield self.testcase_insn(nr, 'ldrsw\t\t{reg}, {label}'.format(**locals()))
        yield state.check({reg:val})
        yield state.restore()
        
    def test_end(self):
        yield '    .align 2'
        yield '    .text'
        for i in xrange(1, len(self.symbols), 2):
            yield self.symbols[i] + ':'
            yield '    .word %s' % hex(self.randvals[i])
            yield '    .skip %d' % random.randrange(512, 2048, 4)
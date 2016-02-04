import random
from common import *

class test_a64_b(TemplateTest):
    
    def gen_rand(self):
        while True:
            yield {'label_idx': random.randint(0, self.__label_count - 1)}

    def __init__(self):
        self.__label_count = 8
        self.symbols = [ __name__ + '_addr_' + str(i) for i in xrange(self.__label_count) ]
        self.randvals = random.sample(xrange(0, 0xfffffffffffffff), self.__label_count)

    def test_begin(self):
        yield '    .arch armv8-a'
        yield '    .align 2'
        yield '    .text'
        for i in xrange(0, len(self.symbols), 2):
            yield self.symbols[i] + ':'
            yield '    ldr\t\tx0, ={0}'.format(hex(self.randvals[i]))
            yield '    ret'
            yield '    .skip %d' % random.randrange(512, 2048, 4)

    def gen_testcase(self, nr, label_idx):
        label = self.symbols[label_idx]
        ret_label = self.testcase_name(nr) + '_ret'
        state = ProcessorState(setreg={'x30':ret_label}, reserve=['x0'])        
        yield state.prepare()
        yield self.testcase_insn(nr, 'b\t\t{0}'.format(label))
        yield ret_label + ':'
        yield state.check({'x0':self.randvals[label_idx]})
        yield state.restore()
        
    def test_end(self):
        for i in xrange(1, len(self.symbols), 2):
            yield '    .skip %d' % random.randrange(512, 2048, 4)
            yield self.symbols[i] + ':'
            yield '    ldr\t\tx0, ={0}'.format(hex(self.randvals[i]))
            yield '    ret'

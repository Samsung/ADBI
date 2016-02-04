import random
import itertools
from PIL.BmpImagePlugin import SAVE
from posix import setregid
random.seed(0xadb1beef)

REGS64 = ['x%d' % i for i in xrange(31)] + ['sp']
REGS32 = ['w%d' % i for i in xrange(31)] + ['sp']
REGS = REGS64 + REGS32

GPREGS64 = ['x%d' %i for i in xrange(29)]
GPREGS32 = ['w%d' %i for i in xrange(29)]
GPREGS = GPREGS64 + GPREGS32

# Parameter and result registers
PARREGS64 = ['x%d' %i for i in range(0,8)] 
PARREGS32 = ['w%d' %i for i in range(0,8)] 
PARREGS = PARREGS64 + PARREGS32

# Caller saved temporary registers
TMPREGS64 = ['x%d' %i for i in range(9,16)] 
TMPREGS32 = ['w%d' %i for i in range(9,16)] 
TMPREGS = TMPREGS64 + TMPREGS32

# Calee saved registers
CALREGS64 = ['x%d' %i for i in range(19,29)] 
CALREGS32 = ['w%d' %i for i in range(19,29)]
CALREGS = CALREGS64 + CALREGS32

NEEDSAVEREGS64 = ['x%d' %i for i in range(18,31)] + ['sp']
NEEDSAVEREGS32 = ['w%d' %i for i in range(18,31)] + ['sp']
NEEDSAVEREGS = NEEDSAVEREGS64 + NEEDSAVEREGS32

COND = 'eq ne cs cc mi pl vs vc hi ls ge lt gt le al'.split()

def check_nzcv_cond(nzcv, cond):
    if nzcv > 0b1111:
        raise ValueError("Too big NZCV register")
    
    n = bool(nzcv & 0b1000)
    z = bool(nzcv & 0b0100) 
    c = bool(nzcv & 0b0010)
    v = bool(nzcv & 0b0001)

    if cond == 'eq':
        return z
    elif cond == 'ne':
        return not z
    elif cond == 'cs':
        return c
    elif cond == 'cc':
        return not c
    elif cond == 'mi':
        return n
    elif cond == 'pl':
        return not n
    elif cond == 'vs':
        return v
    elif cond == 'vc':
        return not v
    elif cond == 'hi':
        return c and (not z)
    elif cond == 'ls':
        return (not c) or z
    elif cond == 'ge':
        return n == v
    elif cond == 'lt':
        return n != v
    elif cond == 'gt':
        return (not z) and (n == v)
    elif cond == 'le':
        return z or (n != v)
    elif cond == 'al':
        return True
    else:
        raise ValueError("Bad condition: %s" % cond)
        

class ProcessorState(object):

    def __init__(self, save=[], setreg={}, store=[], reserve=[]):
        for r in set(save) | set(setreg.keys()) | set(store):
            if r not in (REGS64 + REGS32 + ['nzcv']):
                raise ValueError("Illegal register name: %s" % r)
        
        self.setreg = setreg
        self.setreg_save = set(['x'+r[1:] if r.startswith('w') else r for r in setreg.keys()])
        
        self.save = set(['x'+r[1:] if r.startswith('w') else r for r in save])
        self.store = set(['x'+r[1:] if r.startswith('w') else r for r in store])
        
        self.used = set(['x'+r[1:] if r.startswith('w') else r for r in reserve]) | self.save | self.store | self.setreg_save
        self.stored = None
        self.sp_reg = None
        self.res = None
        
    def __setregs(self, setregs):
        text = []
        for k, v in setregs.iteritems():
            if k == 'nzcv':
                if isinstance(v, str) and (v.startswith('0x')):
                    v = int(v)
                
                t, free = self.__get_unused()
                text.append(t)
                
                if isinstance(v, int):
                    if v > 0b1111:
                        raise ValueError("Too big NZCV register")
                    text.append('    // NZCV = [%s%s%s%s]' % ('n' if v & 0b1000 else ' ',
                                                              'z' if v & 0b0100 else ' ',
                                                              'c' if v & 0b0010 else ' ',
                                                              'v' if v & 0b0001 else ' '))
                    v <<= 28
                elif v not in (REGS32 + REGS64):
                    raise ValueError("Bad NZCV assignment")
                
                text.append(self.__setregs({free:v}))
                text.append('    msr\t\tnzcv, {0}'.format(free))
                text.append(self.__put_unused(free))
            elif (isinstance(v, int) or isinstance(v, long)):
                text.append('    ldr\t\t{0}, ={1}'.format(k, hex(v).rstrip("L")))
            elif isinstance(v, str):
                if k == v:
                    raise ValueError('Setregs failed! Source and destination register are the same: %s' % k)
                if v.startswith('0x'):
                    text.append('    ldr\t\t{0}, ={1}'.format(k, v))
                elif v in REGS64+REGS32:
                    text.append('    mov\t\t{0}, {1}'.format(k, v))
                elif v.startswith('pageof:'):
                    text.append('    adrp\t{0}, {1}'.format(k, v.split(':')[1]))
                else:
                    text.append('    adr\t\t{0}, {1}'.format(k, v))
            else:
                raise ValueError("Bad assignment type: %s" % str(type(v)))

        return '\n'.join(text)
        
    def __get_unused(self, comment=''):
        text = []
        notneedsave = list(((set(REGS64) - set(PARREGS64)) - self.used) - set(NEEDSAVEREGS64))
        if not notneedsave:
            raise NotImplementedError("Custom register stores on stack not yet implemented")
        
        reg = random.choice(notneedsave)
        self.used.add(reg)
        
        return '    // register %s will be temporary used %s' % (reg, comment), reg
            
    def __put_unused(self, reg, comment=''):
        self.used.remove(reg)
        return '    // register %s will not be used %s' % (reg, comment)
        
    def prepare(self):
        text = []    
        if self.stored:
            raise ValueError("Illegal State")
        
        to_store = list((self.store | self.setreg_save | self.save) & (set(NEEDSAVEREGS64) | set(NEEDSAVEREGS32)))
    
        if ('x0' in to_store) or ('w0' in to_store):
            raise ValueError("x0 register cannot be stored")
    
        if ('x0' in self.used) or ('w0' in self.used):
            t, self.res = self.__get_unused('as result register')
            text.append(t)
        else:
            self.res = 'x0'
    
        text.append('    mov\t\t%s, #0' % self.res)
    
        sp_reg = ''
        if 'sp' in to_store:
            sp_reg = self.__get_unused("as stack poiner holder")
            to_store.append(sp_reg)
            to_store.remove('sp')
    
        to_store = sorted(['x'+r[1:] for r in to_store])
        if to_store:            
            text.append('    // store dirty registers: ' + ', '.join(to_store))
            for i in range(0, len(to_store), 2):
                if (i+1) < len(to_store):
                    text.append('    stp\t\t{0}, {1}, [sp, #-0x10]!'.format(to_store[i], to_store[i+1]))
                else:
                    text.append('    str\t\t{0}, [sp, #-0x10]!'.format(to_store[i]))
        
        if sp_reg:
            text.append('    // store sp register in {0}'.format(sp_reg))
            text.append('    mov\t\t{0}, sp')
    
        if self.setreg:
            text.append('    // set register values: ' + ', '.join(['{0}={1}'.format(k, v if isinstance(v, str) else hex(v)) for k, v in self.setreg.iteritems()]))
            text.append(self.__setregs(self.setreg))
                
    
        self.stored = to_store
        self.sp_reg = sp_reg
        
        if not text:
            return '    // nothing to prepare'
        
        return '\n'.join(text)
                
    def restore(self):
        to_restore = self.stored
        text = []
        if self.sp_reg:
            text.append('    // restore sp first')
            text.append('    mov\t\tsp, {0}'.format(sp_reg))
            text.append(self.__put_unused(self.sp_reg, "as stack pointer holder"))

        if self.res is not 'x0':        
            text.append('    mov\t\tx0, {0}'.format(self.res))
            text.append(self.__put_unused(self.res, 'as result register'))
            
        if to_restore:
            text.append('    // restore dirty registers: ' + ', '.join(to_restore))
            
            l = len(to_restore)
            if (l % 2 == 1):
                text.append('    ldr\t\t{0}, [sp], #0x10'.format(to_restore[l-1]))
                l -= 1        
            for i in range(0, l, 2)[::-1]:
                text.append('    ldp\t\t{0}, {1}, [sp], #0x10'.format(to_restore[i], to_restore[i+1]))
            
        self.stored = None
        
        if not text:
            return '    // nothing to restore'

        return '\n'.join(text)
         
    def check(self, state):
        if isinstance(self.stored, type(None)):
            raise ValueError("Illegal State")
    
        text = []
        text.append('    // check register values: ' + ', '.join(['{0}={1}'.format(k, v if isinstance(v, str) else hex(v)) for k, v in state.iteritems()]))
        t, free = self.__get_unused()
        text.append(t)
        for reg, val in state.iteritems():
            text.append(self.__setregs({free: val}))
            if reg.startswith('x') or reg == 'sp':
                text.append('    eor\t\t{1}, {0}, {1}'.format(reg, free))
                text.append('    eor\t\t{0}, {0}, {1}'.format(self.res, free))
            elif reg.startswith('w'):
                text.append('    eor\t\t{1}, {0}, {1}'.format('w'+reg[1:], 'w'+free[1:]))
                text.append('    eor\t\t{0}, {0}, {1}'.format('w'+self.res[1:], 'w'+free[1:]))
            else:
                raise ValueError("Illegal register name: %s" % reg)
        text.append(self.__put_unused(free))

        return '\n'.join(text)

class TemplateTest(object):
    
    count = 512
    __nr = 0
    
    def generate(self, asm_file, adbi_file):
        asm_file.write('\n'.join(self.test_begin()))
        adbi_file.write('\n'.join(self.adbi_begin()))
        for data in self.__gen_data(self.count):
             asm_file.write('\n'.join(itertools.chain(
                    [''],
                    self.testcase_begin(**data),
                    self.gen_testcase(**data),
                    self.testcase_end(**data))))
             
             adbi_file.write('\n')
             adbi_file.write('\n'.join(self.gen_adbi(**data)))
             
        asm_file.write('\n')
        asm_file.write('\n'.join(self.test_end()))
        asm_file.write('\n')
        adbi_file.write('\n'.join(self.adbi_end()))
    
    def __gen_data(self, count):
        for i, rand in enumerate(self.gen_rand()):
            type(self).__nr += 1
            yield dict(nr=type(self).__nr, **rand)
            if i >= (count - 1):
                break
            
    def gen_rand(self):
        raise NotImplementedError
    
    def gen_testcase(self, nr, rand):
        raise NotImplementedError
    
    def gen_adbi(self, nr, **kwargs):
        yield '#handler {0}\n#endhandler\n'.format(self.testcase_insn_symbol(nr))
    
    def adbi_begin(self):
        return
        yield
    
    def adbi_end(self):
        return
        yield
    
    
    def test_begin(self):
        yield '    .arch armv8-a'
        yield '    .text'
        
    def test_end(self):
        return
        yield
    
    
    def testcase_name(self, nr):
        return '%s_%04d' % (type(self).__name__, nr)
    
    def testcase_insn_symbol(self, nr):
        return self.testcase_name(nr) + '_tinst'
    
    def testcase_insn(self, nr, insn):
        return '''    .type {0}, %function
{0}:
    {1}'''.format(self.testcase_insn_symbol(nr), insn)
    
    def testcase_begin(self, nr, **kwargs):
        yield '''    .global {0}
    .type {0}, %function
{0}:'''.format(self.testcase_name(nr))

    def testcase_end(self, nr, **kwargs):
        yield '    ret'
        yield '    .size {0}, . - {0}'.format(self.testcase_name(nr))
    

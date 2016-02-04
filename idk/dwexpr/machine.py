from io import BytesIO
import struct

from common.leb128 import LEB, SLEB
from .constants import OPCODES 

from .result import Result

class Machine(object):
    '''DWARF expression evaluating machine.
    
    DWARF expressions are evaluated by executing a series of opcodes (with proper arguments) and operating on a stack. 
    This implementation only supports a small subset of all opcodes.  The supported opcodes are those, which are used
    in typical location descriptions.   
    '''
    
    def __init__(self, expression):
        self.stack = []
        self.origexpr = bytearray(expression)
        self.expression = BytesIO(self.origexpr)
        self.uses_frame = False
        self.uses_cfa = False
        self.result = None

    # Supporting methods
    
    def read(self, count=1, force=True):
        '''Read count bytes of the expression.'''
        ret = self.expression.read(count)
        if force and len(ret) < count:
            raise ValueError('unexpected end of expression')
        return ret
        
    def push(self, value):
        '''Push a value to the DWARF stack.'''
        self.stack.append(value)
        
    def pop(self):
        '''Pop a value from the DWARF stack.'''
        if not self.stack:
            raise ValueError('stack underflow')
        return self.stack.pop()
       
       
    # Special opcodes

    def unsupported(self):
        '''Placeholder method called when an unimplemented opcode is encountered.'''
        raise NotImplementedError
       
    def nop(self):
        '''The DW_OP_nop operation is a place holder.'''
        pass


    # Stack operations

    def dup(self):
        '''The DW_OP_dup operation duplicates the value at the top of the stack.'''
        value = self.pop()
        self.push(value)
        self.push(value)

    def drop(self):
        '''The DW_OP_drop operation pops the value at the top of the stack.'''
        self.pop()

    def pick(self):
        '''The single operand of the DW_OP_pick operation provides a 1-byte index. 
        
        A copy of the stack entry with the specified index (0 through 255, inclusive) is pushed onto the stack.'''
        idx = ord(self.read()) + 1
        try:
            val = self.stack[-idx]
            self.push(val)
        except KeyError:
            raise ValueError('stack reference error')
        
    def over(self):
        '''The DW_OP_over operation duplicates the entry currently second in the stack at the top of the stack.'''
        top = self.pop()
        second = self.pop()
        self.push(second)
        self.push(top)
        self.push(second)
        
    def swap(self):
        '''The DW_OP_swap operation swaps the top two stack entries.'''
        a = self.pop()
        b = self.pop()
        self.push(a)
        self.push(b)
        
    def rot(self):
        '''The DW_OP_rot operation rotates the first three stack entries.
        
        The entry at the top of the stack becomes the third stack entry, the second entry becomes the top of the stack, 
        and the third entry becomes the second entry.'''
        a = self.pop()
        b = self.pop()
        c = self.pop()
        self.push(a)
        self.push(c)
        self.push(b)
         
    def call_frame_cfa(self):
        '''The DW_OP_call_frame_cfa operation pushes the value of the CFA, obtained from the Call
        Frame Information (see Section 6.4).'''
        self.push('__adbi_cfa__')
        self.uses_cfa = True

         
    # Literals
     
    def addr(self):
        '''The DW_OP_addr operation has a single operand that encodes a machine address and whose size is the size of 
        an address on the target machine.'''
        address = struct.unpack('<I', self.read(4))       
        self.push('(void *) %#010x' % address)
        
    def constXX(self, fmt):
        '''The single operand of a DW_OP_constnu operation provides a 1, 2, 4, or 8-byte unsigned integer constant, 
        respectively.'''
        fmt = '<' + fmt
        size = struct.calcsize(fmt)
        value = struct.unpack(fmt, size)
        self.push('%#x' % value)
        
    const1u = lambda self: self.constXX('B') 
    const1s = lambda self: self.constXX('b')
    const2u = lambda self: self.constXX('H') 
    const2s = lambda self: self.constXX('h')    
    const4u = lambda self: self.constXX('I') 
    const4s = lambda self: self.constXX('i')
    const8u = lambda self: self.constXX('Q') 
    const8s = lambda self: self.constXX('q')

    def constu(self):
        '''The single operand of the DW_OP_constu operation provides an unsigned LEB128 integer constant.'''
        val = LEB.decode(self.expression)
        self.push('%#x' % val)
        
    def consts(self):
        '''The single operand of the DW_OP_consts operation provides a signed LEB128 integer constant.'''
        val = SLEB.decode(self.expression)
        self.push('%#x' % val)

    def litX(self, value):
        '''The DW_OP_litn operations encode the unsigned literal values from 0 through 31, inclusive.'''
        self.push('%#x' % value)

    lit0 = lambda self: self.litX(0)
    lit1 = lambda self: self.litX(1)
    lit2 = lambda self: self.litX(2)
    lit3 = lambda self: self.litX(3)
    lit4 = lambda self: self.litX(4)
    lit5 = lambda self: self.litX(5)
    lit6 = lambda self: self.litX(6)
    lit7 = lambda self: self.litX(7)
    lit8 = lambda self: self.litX(8)
    lit9 = lambda self: self.litX(9)
    lit10 = lambda self: self.litX(10)
    lit11 = lambda self: self.litX(11)
    lit12 = lambda self: self.litX(12)
    lit13 = lambda self: self.litX(13)
    lit14 = lambda self: self.litX(14)
    lit15 = lambda self: self.litX(15)
    lit16 = lambda self: self.litX(16)
    lit17 = lambda self: self.litX(17)
    lit18 = lambda self: self.litX(18)
    lit19 = lambda self: self.litX(19)
    lit20 = lambda self: self.litX(20)
    lit21 = lambda self: self.litX(21)
    lit22 = lambda self: self.litX(22)
    lit23 = lambda self: self.litX(23)
    lit24 = lambda self: self.litX(24)
    lit25 = lambda self: self.litX(25)
    lit26 = lambda self: self.litX(26)
    lit27 = lambda self: self.litX(27)
    lit28 = lambda self: self.litX(28)
    lit29 = lambda self: self.litX(29)
    lit30 = lambda self: self.litX(30)
    lit31 = lambda self: self.litX(31)
    
        
    # Register based addressing
    
    def fbreg(self):
        '''The DW_OP_fbreg operation provides a signed LEB128 offset from the address specified by the location
        description in the DW_AT_frame_base attribute of the current function. (This is typically a "stack pointer" 
        register plus or minus some offset. On more sophisticated systems it might be a location list that adjusts
        the offset according to changes in the stack pointer as the PC changes.)'''
        offset = SLEB.decode(self.expression)
        if offset > 0:
            self.push('__adbi_frame__ + %#x' % offset)
        elif offset < 0:
            self.push('__adbi_frame__ - %#x' % abs(offset))
        else:
            self.push('__adbi_frame__')
        self.uses_frame = True

    def bregN(self, regnum):
        '''The single operand of the DW_OP_bregn operations provides a signed LEB128 offset from the specified 
        register.'''
        offset = SLEB.decode(self.expression)
        
        if offset > 0:
            self.push('(char *) get_reg(%i) + %#x' % (regnum, offset))            
        elif offset < 0:
            self.push('(char *) get_reg(%i) - %#x' % (regnum, abs(offset)))
        else:
            self.push('(char *) get_reg(%i)' % (regnum))
    
    breg0 = lambda self: self.bregN(0)
    breg1 = lambda self: self.bregN(1)
    breg2 = lambda self: self.bregN(2)
    breg3 = lambda self: self.bregN(3)
    breg4 = lambda self: self.bregN(4)
    breg5 = lambda self: self.bregN(5)
    breg6 = lambda self: self.bregN(6)
    breg7 = lambda self: self.bregN(7)
    breg8 = lambda self: self.bregN(8)
    breg9 = lambda self: self.bregN(9)
    breg10 = lambda self: self.bregN(10)
    breg11 = lambda self: self.bregN(11)
    breg12 = lambda self: self.bregN(12)
    breg13 = lambda self: self.bregN(13)
    breg14 = lambda self: self.bregN(14)
    breg15 = lambda self: self.bregN(15)
    breg16 = lambda self: self.bregN(16)
    breg17 = lambda self: self.bregN(17)
    breg18 = lambda self: self.bregN(18)
    breg19 = lambda self: self.bregN(19)
    breg20 = lambda self: self.bregN(20)
    breg21 = lambda self: self.bregN(21)
    breg22 = lambda self: self.bregN(22)
    breg23 = lambda self: self.bregN(23)
    breg24 = lambda self: self.bregN(24)
    breg25 = lambda self: self.bregN(25)
    breg26 = lambda self: self.bregN(26)
    breg27 = lambda self: self.bregN(27)
    breg28 = lambda self: self.bregN(28)
    breg29 = lambda self: self.bregN(29)
    breg30 = lambda self: self.bregN(30)
    breg31 = lambda self: self.bregN(31)
    
    def bregx(self):
        '''The DW_OP_bregx operation has two operands: a register which is specified by an unsigned LEB128 number, 
        followed by a signed LEB128 offset.'''
        regnum = LEB.decode(self.expression)
        return self.bregN(regnum)

    # Arithmetic operations
    
    # TODO: There are many arithmetic operations defined by the DWARF standard, but they are rarely used in location 
    #       descriptions.  The following ones are implemented just as examples. 
    
    def minus(self):
        '''The DW_OP_minus operation pops the top two stack values, subtracts the former top of the stack from the 
        former second entry, and pushes the result.'''
        a = self.pop()
        b = self.pop()
        self.push('(void *) ((char *) (%s)) - ((char *) (%s))' % (a, b))
    
    def plus(self):
        '''The DW_OP_plus operation pops the top two stack entries, adds them together, and pushes the result.'''
        a = self.pop()
        b = self.pop()
        self.push('(void *) ((char *) (%s)) + ((char *) (%s))' % (a, b))
    
    
    # Direct register references
    
    def regx(self):
        regnum = LEB.decode(self.expression)
        return 'get_reg(%i)' % regnum
        
    def regN(self, n):
        regnum = n
        return 'get_reg(%i)' % regnum

    reg0 = lambda self: self.regN(0)
    reg1 = lambda self: self.regN(1)
    reg2 = lambda self: self.regN(2)
    reg3 = lambda self: self.regN(3)
    reg4 = lambda self: self.regN(4)
    reg5 = lambda self: self.regN(5)
    reg6 = lambda self: self.regN(6)
    reg7 = lambda self: self.regN(7)
    reg8 = lambda self: self.regN(8)
    reg9 = lambda self: self.regN(9)
    reg10 = lambda self: self.regN(10)
    reg11 = lambda self: self.regN(11)
    reg12 = lambda self: self.regN(12)
    reg13 = lambda self: self.regN(13)
    reg14 = lambda self: self.regN(14)
    reg15 = lambda self: self.regN(15)
    reg16 = lambda self: self.regN(16)
    reg17 = lambda self: self.regN(17)
    reg18 = lambda self: self.regN(18)
    reg19 = lambda self: self.regN(19)
    reg20 = lambda self: self.regN(20)
    reg21 = lambda self: self.regN(21)
    reg22 = lambda self: self.regN(22)
    reg23 = lambda self: self.regN(23)
    reg24 = lambda self: self.regN(24)
    reg25 = lambda self: self.regN(25)
    reg26 = lambda self: self.regN(26)
    reg27 = lambda self: self.regN(27)
    reg28 = lambda self: self.regN(28)
    reg29 = lambda self: self.regN(29)
    reg30 = lambda self: self.regN(30)
    reg31 = lambda self: self.regN(31)
    

    # Implicit location description

    def stack_value(self):
        '''The DW_OP_stack_value operation specifies that the object does not exist in memory but its
        value is nonetheless known and is at the top of the DWARF expression stack. In this form of
        location description, the DWARF expression represents the actual value of the object, rather
        than its location. The DW_OP_stack_value operation terminates the expression.'''
        return self.pop()

    
    def run(self):
        '''Evaluate the DWARF expression and return a Result object.'''
        while True:
            opcode = self.read(force=False)
            if not opcode:
                break
            
            try:
                opcode_name = OPCODES[ord(opcode)]
            except KeyError:
                raise ValueError('malformed DWARF expression -- invalid opcode %02x' % ord(opcode))
            
            fn = getattr(self, opcode_name, self.unsupported)
            ret = fn()
            
            if ret is not None:
                return Result(ret, False, self.uses_frame, self.uses_cfa)
            
        return Result(self.pop(), True, self.uses_frame, self.uses_cfa)


def eval_DWARF_expr(expr_code):
    '''Evaluate the given DWARF expression code and return the result.'''
    machine = Machine(expr_code)
    return machine.run()

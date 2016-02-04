from base64 import b64decode

from .base import BinaryElementBase
from .location import UndefinedLocation
from common.deco import cachedprop, cachedfn
from common.mixin import EqualityMixin
from dwexpr import eval_DWARF_expr 

class Variable(BinaryElementBase, EqualityMixin):
    def __init__(self, elf, idx):
        BinaryElementBase.__init__(self, elf)
        
        self.idx = idx

        query = 'select type, name, global, loc from variables where id=?'
        self.type_id, self.name, self.is_global, self.loc_id = self.query_db_one(query, idx)

        self.is_global = bool(self.is_global)

    @property 
    def is_scalar(self):
        '''True if the datatype of this variable is scalar.'''
        return self.datatype.is_scalar

    @cachedprop
    def location(self):
        if self.loc_id is not None:
            return self.elf.locations[self.loc_id]
        else:
            return UndefinedLocation 

    @cachedprop
    def datatype(self):
        return self.elf.types[self.type_id]

    @cachedprop
    def expressions(self):
        return list(self.query_db('select lo, hi, expr from variables2expressions where var=?', self.idx))

    def get_expression(self, addr):
        if not self.visible_at(addr):
            return None
        for low, high, expr in self.expressions:
            if low is None:
                return b64decode(expr)
            else:
                if low <= addr < high:
                    return b64decode(expr)

    @cachedprop
    def ranges(self):
        query = 'select lo, hi from variables2ranges where var=?'  
        self.query_db(query, self.idx)
        
    def visible_at(self, addr):
        '''Check if the given variable is accessible at the given address.'''
        if self.is_global:
            return True
        if not self.ranges:
            return True
        for low, high in self.ranges:
            if low <= addr < high:
                return True 
        return False
    
    def accessible_at(self, addr):
        '''Check if the given variable is accessible for debugging at the given address.'''
        return self.get_expression(addr) is not None
        
    def __str__(self):
        return '%s %s' % (str(self.datatype), self.name if self.name else '<anon>') 

    def __hash__(self):
        return self.idx
    
    def __eq__(self, other):
        return self._clseq(other) and (self.idx == other.idx)

    def iter_declaration(self, name=None):
        for line in self.datatype.define_var(name or self.name):
            yield line

    def getvar(self, addr, name=None):
        return eval_DWARF_expr(self.get_expression(addr))


class Variables(BinaryElementBase):
    def __init__(self, elf):
        BinaryElementBase.__init__(self, elf)

    @cachedfn
    def __getitem__(self, idx):
        try:
            return Variable(self.elf, idx)
        except ValueError:
            raise KeyError

    def __iter__(self):
        '''Yield all variables.'''
        for idx in self.query_db('select id from variables'):
            yield self[idx] 

    def iter_globals(self):
        '''Yield global variables.'''
        for idx in self.query_db('select id from variables where global=1'):
            yield self[idx] 

    def iter_locals(self, addr):
        '''Yield local variables visible at the given address.'''
        
        query = '''select var from variables2ranges 
        where lo <= ? and ? < hi
        and var in (select id from variables where global=0)
        '''
        for idx in self.query_db(query, addr, addr):
            yield self[idx]
            
    def iter_variables(self, addr):
        '''Yield all variables visible at the given address.''' 
        for var in self.iter_locals(addr):
            yield var
        for var in self.iter_globals():
            yield var
            
    def lookup(self, addr, name):
        '''Find a variable matching the given name visible at the given address.''' 
        for var in self.iter_variables(addr):
            if var.name == name:
                return var


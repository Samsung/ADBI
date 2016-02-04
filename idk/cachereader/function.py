from base64 import b64decode

from .base import BinaryElementBase
from common.deco import cachedprop, cachedfn
from dwexpr import eval_DWARF_expr

class Function(BinaryElementBase):
    def __init__(self, elf, idx, name, lo, hi, loc):
        BinaryElementBase.__init__(self, elf)
        self.name = name
        self.lo = lo
        self.hi = hi
        self.loc_id = loc
        self.idx = idx
 
    @property
    def location(self):
        if self.loc_id is not None:
            return self.elf.locations[self.loc_id]

    @cachedprop
    def expressions(self):
        return list(self.query_db('select lo, hi, expr from framepointers where func=?', self.idx))

    def get_expression(self, addr):
        for low, high, expr in self.expressions:
            if low is None:
                return b64decode(expr)
            else:
                if low <= addr < high:
                    return b64decode(expr)

    def getframe(self, addr):
        return eval_DWARF_expr(self.get_expression(addr))

    def iter_locations(self):
        '''Yield address-location pairs inside the function.'''
        return self.elf.lines.iter_locations(self.lo, self.hi)

    @cachedprop
    def first_insn(self):
        '''Address of first instruction after the preamble.

        The address is evaluated heuristically.  It is the second address inside the function, for 
        which a line information entry exists.
        '''
        for n, (addr, dummy) in enumerate(self.iter_locations(), 1):
            if n >= 2:
                return addr
        return self.lo

    @cachedprop
    def params(self):
        '''Return a list of parameter variables.'''
        query = 'select var from params where func=? order by idx'
        return [self.elf.variables[each] for each in self.query_db(query, self.idx)]


class Functions(BinaryElementBase):

    @cachedfn
    def __getitem__(self, idx):
        query = 'select name, lo, hi, loc from functions where id = ?'
        name, lo, hi, loc = self.query_db_one(query, idx)
        return Function(self.elf, idx, name, lo, hi, loc)


    def __iter__(self):
        query = 'select id from functions'
        for idx in self.query_db(query):
            yield self[idx]



from base64 import b64decode

from .base import BinaryElementBase
from common.deco import cachedfn
from dwexpr import eval_DWARF_expr

class Framepointers(BinaryElementBase):

    @cachedfn
    def __getitem__(self, addr):
        # Search by frame pointer range
        query = '''select framepointers.expr from framepointers 
        where lo <= ? and ? < hi'''
        ret = self.query_db_one(query, addr, addr)
        if ret is not None:
            return b64decode(ret)

        # Search by function 
        query = '''select framepointers.expr 
        from framepointers join functions 
        on framepointers.func = functions.id 
        where functions.lo <= ? and ? < functions.hi and framepointers.lo is null'''
        ret = self.query_db_one(query, addr, addr)
        if ret is not None:
            return b64decode(ret)

        # Failed
        raise KeyError
             

    def getframe(self, addr):
        return eval_DWARF_expr(self[addr])

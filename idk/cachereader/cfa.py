from .base import BinaryElementBase
from dwexpr import eval_DWARF_expr

from base64 import b64decode

class CallFrameAddress(BinaryElementBase):
    
    def get_expression_code(self, addr):
        query = '''select expr from cfi where lo <= ? and ? < hi limit 1'''
        ret = self.query_db_one(query, addr, addr)
        if ret is None:
            raise KeyError
        else:
            return b64decode(ret)
            
    def get_expression(self, addr):
        return eval_DWARF_expr(self.get_expression_code(addr))
    
    def __getitem__(self, addr):
        return self.get_expression(addr)
        

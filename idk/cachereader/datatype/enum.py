from ..C import INDENT
from .datatype import DataType, TypeInfo
from common.deco import cachedprop


class EnumType(DataType):

    def _get_is_scalar(self):
        return True
    
    class Enumerator(TypeInfo):
        def __init__(self, elf, eid):
            query = 'select parent, name, value, loc from enumerators where id=?'
            parent_id, name, self.value, loc_id = elf.query_db_one(query, eid)
            TypeInfo.__init__(self, elf, parent_id, name, loc_id)

    def define_enumerators(self):
        if self.enumerators:
            nl = max(len(enumeration.name) for enumeration in self.enumerators)
            xl = max(len(hex(enumeration.value)) for enumeration in self.enumerators) - 2
            dl = max(len(str(enumeration.value)) for enumeration in self.enumerators)
            for enumeration in self.enumerators:
                n = enumeration.name
                v = enumeration.value
                yield '%-*s = 0x%0*x,   /* == %*i */' % (nl, n, xl, v, dl, v)
        else:
            yield '/* no enumerators */'

    @cachedprop
    def enumerators(self):
        return [self.Enumerator(self.elf, eid) for eid in self.query_db('select id from enumerators where parent=?', self.tid)]


    def _define_var_type(self):
        if self.is_anonymous:
            yield 'enum {' 
            for line in self.define_enumerators():
                yield INDENT + line
            yield '}'
        else:
            yield 'enum %s' % self.codename


    def define(self):
        if not self.is_anonymous:
            yield 'enum %s {' % self.codename
            for line in self.define_enumerators():
                yield INDENT + line
            yield '};'
        else:
            # Type anonymous, no need to define
            pass

    def __str__(self):
        return 'enum %s' % self.safename
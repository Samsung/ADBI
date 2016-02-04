from ..C import INDENT
from common.deco import cachedprop
from .datatype import DataType, TypeInfo


class CompoundType(DataType):

    COMPOUND_PREFIX = 'struct /* unknown compound type */'
    STR_PREFIX = 'compound'

    def _get_is_scalar(self):
        return False

    class Member(TypeInfo):
        def __init__(self, elf, mid):
            query = 'select parent, type, offset, name, loc from members where id=?'
            parent_id, self.type_id, self.offset, name, loc_id = elf.query_db_one(query, mid)
            TypeInfo.__init__(self, elf, parent_id, name, loc_id)

        @property 
        def parent(self):
            return self.elf.types[self.parent_id]

        @property 
        def datatype(self):
            return self.elf.types[self.type_id]

    def define_members(self):
        offset = 0
        num = 0
        for member in self.members:
            if offset < member.offset / 8:
                padbytes = member.offset / 8 - offset
                yield 'char __adbi_member_padding_%06x_%02i__[%i];' % (self.tid, num, padbytes)
                num += 1
                offset = member.offset
            if member.location:
                yield '/* Original definition: %s */' % member.location
            yield '/* Field offset: %4i */' % offset 
            for line in member.datatype.define_var(member.name):
                yield line
            offset += member.datatype.byte_size
            yield ''
            

    @cachedprop
    def members(self):
        members = (self.Member(self.elf, mid) for mid in self.query_db('select id from members where parent=?', self.tid))
        return sorted(members)

    def _define_var_type(self):
        if self.is_anonymous:
            yield '%s __attribute__ ((__packed__)) {' % self.COMPOUND_PREFIX
            for line in self.define_members():
                yield INDENT + line
            yield '}'
        else:
            yield self.codename

    def define(self):
        if not self.is_anonymous:
            yield '%s __attribute__ ((__packed__)) %s {' % (self.COMPOUND_PREFIX, self.name)
            for line in self.define_members():
                yield INDENT + line
            yield '};'
        else:
            # DataType anonymous, no need to define
            pass

    def declare(self):
        if self.is_anonymous:
            raise NotImplementedError
        else:
            yield '%s;' % (self.codename)

    def get_codename(self):
        if self.is_anonymous:
            return '__adbi_anon_type_%06x' % self.tid
        else:
            return '%s %s' % (self.COMPOUND_PREFIX, self.name)

    def get_hard_requirements(self):
        def iter_req():
            for member in self.members:
                if member.datatype.is_anonymous:
                    for req in member.datatype.hard_requirements:
                        yield req
                else:
                    yield member.datatype
        return set(iter_req())
                    
    def get_soft_requirements(self):
        def iter_req():
            for member in self.members:
                if member.datatype.is_anonymous:
                    for req in member.datatype.soft_requirements:
                        yield req
                else:
                    # we got a hard requirement on this type
                    pass
        return set(iter_req())

    def __str__(self):
        return '%s %s' % (self.STR_PREFIX, self.safename)

class StructType(CompoundType): 
    COMPOUND_PREFIX = 'struct'
    STR_PREFIX = 'struct'

class ClassType(CompoundType): 
    COMPOUND_PREFIX = 'struct /* class */'
    STR_PREFIX = 'class'

class UnionType(CompoundType): 
    COMPOUND_PREFIX = 'union'
    STR_PREFIX = 'union'
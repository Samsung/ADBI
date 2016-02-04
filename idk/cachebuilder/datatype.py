from io import BytesIO
import hashlib
import logging
from itertools import count

from elftools import dwarf
from elftools.dwarf import dwarf_expr 

from .dwarftools import get_die_offset_by_reference, get_attr_val
from common.enums import TypeKinds
from common.leb128 import LEB

hashfn = hashlib.sha256


class TypeReference(object):
    def __init__(self, die, refattr, types):        
        self.types = types 
        self.offset = get_die_offset_by_reference(die, refattr)

    def resolve(self):
        if self.offset is not None:
            return self.types.typemap.get(self.offset, None)
        else:
            return None


def decode_offset(member_location):
    '''Convert dwarf expression in DW_AT_data_member_location to offset from structure start.'''
    if member_location is None:
        # the attribute does not exist -- the member is located at the beginning of its parent
        return 0
    elif isinstance(member_location, int):
        # the attribute is a number -- this is the offset
        return member_location
    elif isinstance(member_location, list):
        # the attribute is an expression
        dwxpr = member_location
        if dwxpr[0] == dwarf_expr.DW_OP_name2opcode['DW_OP_plus_uconst']:
            # Right now we only support location descriptions in the form [DW_OP_plus_uconst, <ULEB128>].
            # This is the only form GCC and clang produce.
            return LEB.decode(BytesIO(bytearray(dwxpr[1:])))
        else:
            opcode = dwarf_expr.DW_OP_opcode2name.get(dwxpr[0], '<unknown>')
            raise NotImplementedError('Unsupported opcode in data member location %s (%02x)' % (opcode, dwxpr[0]))


class Member(object):

    def __eval_offset(self):
        bitoffset = get_attr_val(self.die, 'DW_AT_data_bit_offset')
        if bitoffset is not None:
            # Bit offset is given directly
            return bitoffset

        location = get_attr_val(self.die, 'DW_AT_data_member_location')
        if location is None:
            # no offset attribute exists -- the member is located at the beginning of its parent
            return 0
        elif isinstance(location, (int, long)):
            # the attribute is a number -- this is the offset in bytes
            return location * 8
        elif isinstance(location, list):
            # the attribute is an expression
            dwxpr = location
            if dwxpr[0] == dwarf_expr.DW_OP_name2opcode['DW_OP_plus_uconst']:
                # Right now we only support location descriptions in the form [DW_OP_plus_uconst, <ULEB128>].
                # This is the only form GCC and clang produce.
                return LEB.decode(BytesIO(bytearray(dwxpr[1:]))) * 8
            else:
                opcode = dwarf_expr.DW_OP_opcode2name.get(dwxpr[0], '<unknown>')
                raise NotImplementedError('Unsupported opcode in data member location %s (%02x)' % (opcode, dwxpr[0]))

        assert False, 'Unreachable code.'

    def __init__(self, parent, die):
        assert die.tag == 'DW_TAG_member'
        self.die = die
        self.parent = parent
        self.type_ref = TypeReference(die, 'DW_AT_type', parent.types)
        self.name = get_attr_val(die, 'DW_AT_name')
        self.offset = self.__eval_offset()
        self.loc_id = parent.types.di.locations.insert_DIE_flc(die)

    @property
    def datatype(self):
        return self.type_ref.resolve()

    @property
    def db_tuple(self):
        return (self.parent.compressed_id, self.datatype.compressed_id, self.offset, self.name, self.loc_id)

    def store(self, conn):
        query = 'insert into members (parent, type, offset, name, loc) values (?, ?, ?, ?, ?)'
        sid = self.parent.types.simple_id
        values = (sid(self.parent), sid(self.datatype), self.offset, self.name, self.loc_id)
        conn.execute(query, values)


class Enumerator(object):
    def __init__(self, parent, die):
        self.die = die
        self.parent = parent 
        self.name = get_attr_val(die, 'DW_AT_name')
        self.value = get_attr_val(self.die, 'DW_AT_const_value')
        self.loc_id = parent.types.di.locations.insert_DIE_flc(die)

    @property
    def db_tuple(self):
        return (self.parent.compressed_id, self.name, self.value, self.loc_id)


class DataType(object):

    @staticmethod
    def __get_type_kind(die):

        if die.tag == 'DW_TAG_base_type':
            return {
                dwarf.constants.DW_ATE_signed: TypeKinds.int,
                dwarf.constants.DW_ATE_unsigned: TypeKinds.uint,
                dwarf.constants.DW_ATE_signed_char: TypeKinds.char,
                dwarf.constants.DW_ATE_unsigned_char: TypeKinds.uchar,
                dwarf.constants.DW_ATE_float: TypeKinds.float,
                dwarf.constants.DW_ATE_boolean: TypeKinds.bool,
            }.get(die.attributes['DW_AT_encoding'].value, TypeKinds.unsupported)
        else:
            return {
                # base types
                'DW_TAG_base_type': TypeKinds,

                # modified types
                'DW_TAG_const_type': TypeKinds.const,
                'DW_TAG_packed_type': TypeKinds.packed,
                'DW_TAG_pointer_type': TypeKinds.ptr,
                'DW_TAG_reference_type': TypeKinds.ref,
                'DW_TAG_restrict_type': TypeKinds.restrict,
                'DW_TAG_rvalue_reference_type': TypeKinds.rref,
                'DW_TAG_shared_type': TypeKinds.shared,
                'DW_TAG_volatile_type': TypeKinds.volatile,

                # typedefs
                'DW_TAG_typedef': TypeKinds.typedef,

                # compound types
                'DW_TAG_union_type': TypeKinds.union,
                'DW_TAG_structure_type': TypeKinds.struct,
                'DW_TAG_class_type': TypeKinds.cls,

                # enumeration types
                'DW_TAG_enumeration_type': TypeKinds.enum,

                # array types
                'DW_TAG_array_type': TypeKinds.array,
            }.get(die.tag, TypeKinds.unsupported)


    def __init__(self, types, die):
        self.types = types
        self.die = die
        self.tid = types.id_gen.next()
        self.kind = self.__get_type_kind(die)
        self.name = get_attr_val(die, 'DW_AT_name')
        self.bytes = get_attr_val(die, 'DW_AT_byte_size')
        self.inner_ref = TypeReference(die, 'DW_AT_type', types)
        self.loc_id = self.types.di.locations.insert_DIE_flc(die)

        self.members = [Member(self, die) for die in self.die.iter_children() if die.tag == 'DW_TAG_member']
        self.enumerators = [Enumerator(self, die) for die in self.die.iter_children() if die.tag == 'DW_TAG_enumerator']
        
        if self.kind == TypeKinds.array:
            # This is an array
            subrangedies = (cd for cd in self.die.iter_children() if cd.tag == 'DW_TAG_subrange_type')
            def get_limit(die):
                try:
                    attr = die.attributes['DW_AT_upper_bound']
                    if attr.form.startswith('DW_FORM_block'):
                        # TODO: DWARF expression, rare and unimplemented
                        return None
                    else:
                        ret = attr.value 
                        if ret == (1 << 64) - 1:
                            # TODO: The upper bound is -1, this is sometimes generated by G++, but I'm not sure why...
							# Let's pretend it's None
                            return None 
                        return ret + 1
                except KeyError:
                    return None
            self.dimensions = [get_limit(sr) for sr in subrangedies]
        else:
            self.dimensions = []

    @property
    def inner(self):
        return self.inner_ref.resolve()

    @property
    def compressed_inner(self):
        if self.inner:
            return self.types.typehash[self.inner.typehash]

    @property
    def compressed_inner_id(self):
        if self.inner:
            return self.inner.compressed_id

    @property
    def db_tuple(self):
        return (self.tid, self.kind, self.name, self.bytes, self.compressed_inner_id, self.loc_id)

    def store(self, conn):
        query = 'insert into types (id, kind, name, bytes, inner, loc) values (?, ?, ?, ?, ?, ?)'
        items = (self.tid, self.kind, self.name, self.bytes, self.types.simple_id(self.inner), self.loc_id)
        print query, items
        conn.execute(query, items)
        for member in self.members:
            member.store(conn)
        for enum in self.enumerators:
            enum.store(conn)

        if self.kind == TypeKinds.array:
            query = 'insert into array_dim (id, num, size) values (?, ?, ?)'
            values = ((self.tid, i, d) for i, d in enumerate(self.dimensions))
            conn.executemany(query, values)


    def _hash_list(self, backrefs=None):
        
        if backrefs is None:
            backrefs = {}
            
        if self in backrefs:
            yield '%%' + str(backrefs[self]) 
        else:            
            backrefs[self] = len(backrefs)
            
            yield self.kind
            yield self.name 
            yield self.bytes
            if self.inner:
                yield '->'
                for each in self.inner._hash_list(backrefs):
                    yield each
            if self.dimensions:
                yield 'x'.join(str(d) for d in self.dimensions)
            if self.members:
                yield '{'
                for member in self.members:
                    yield member.name
                    yield '~' 
                    for each in member.datatype._hash_list(backrefs):
                        yield each
                yield '}'
            if self.enumerators:
                yield '{'
                for enumerators in self.enumerators:
                    yield enumerators.name
                    yield '~' 
                    yield enumerators.value
                yield '}'

    @property
    def typehash(self):
        '''Hash of the data type.'''
        items = (str(e) for e in self._hash_list())     
        return hashfn(':'.join(items)).digest()

    @property
    def compressed_id(self):
        return self.types.typehash[self.typehash].tid


class DataTypes:
    TAGS = set([
        # base types
        'DW_TAG_base_type',

        # modified types
        'DW_TAG_const_type',
        'DW_TAG_packed_type',
        'DW_TAG_pointer_type',
        'DW_TAG_reference_type',
        'DW_TAG_restrict_type',
        'DW_TAG_rvalue_reference_type',
        'DW_TAG_shared_type',
        'DW_TAG_volatile_type',

        # typedefs
        'DW_TAG_typedef',

        # compound types
        'DW_TAG_union_type',
        'DW_TAG_structure_type',
        'DW_TAG_class_type',

        # enumeration types
        'DW_TAG_enumeration_type',

        # arrays
        'DW_TAG_array_type',
        'DW_TAG_subrange_type',

        # function types
        'DW_TAG_subroutine_type',

        # types not available in C and C++
        'DW_TAG_interface_type',
        'DW_TAG_string_type',
    ])

    def __init__(self, di):
        self.di = di
        self.id_gen = count()
        self.typemap = {}

        for cu in di.dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag in self.TAGS:
                    self.typemap[die.offset] = DataType(self, die)

        self.typehash = {datatype.typehash: datatype for datatype in self.typemap.itervalues()}

        if len(self.typehash) != len(self.typemap):
            logging.debug('Type compression ratio %2.2f%%.', (100.0 * len(self.typehash) / len(self.typemap)))
        else:
            logging.debug('No type compression.')


    def __iter_compressed_types(self):
        '''Yield (compressed) types in declaration order.

        The yield types are partially sorted in such way that the following condition is satisfied:
            If type A is an inner type of type B, type A is yield before type B.

        This kind of sorting is necessary when the types are stored in a database, because the types table's inner 
        column is a foreign key to types.  
        '''
        done = set()
        
        def iter_related(datatype):
            if datatype not in done:
                done.add(datatype)
                if datatype.compressed_inner:
                    for each in iter_related(datatype.compressed_inner):
                        yield each
                yield datatype           
            
        for datatype in self.typehash.itervalues():
            for each in iter_related(datatype):
                yield each
            

    def store(self, conn):
        # Store types
        logging.debug('Storing %i types.', len(self.typehash))
        query = 'insert into types (id, kind, name, bytes, inner, loc) values (?, ?, ?, ?, ?, ?)'
        items = (datatype.db_tuple for datatype in self.__iter_compressed_types())
        conn.executemany(query, items)

        # Store members of compound types
        logging.debug('Storing compound type members.')
        query = 'insert into members (parent, type, offset, name, loc) values (?, ?, ?, ?, ?)'
        items = (member.db_tuple for datatype in self.typehash.itervalues() for member in datatype.members)
        conn.executemany(query, items)
            
        # Store enumerators 
        logging.debug('Storing enumerator values of enumeration types.')
        query = 'insert into enumerators (parent, name, value, loc) values (?, ?, ?, ?)'
        items = (enumerator.db_tuple for datatype in self.typehash.itervalues() for enumerator in datatype.enumerators)
        conn.executemany(query, items)
        
        # Store array dimensions
        logging.debug('Storing array type dimensions.')
        query = 'insert into array_dim (id, num, size) values (?, ?, ?)'
        items = ((datatype.tid, n, size) 
                 for datatype in self.typehash.itervalues() 
                 for n, size in enumerate(datatype.dimensions))
        conn.executemany(query, items)
        
        # Commit changes
        conn.commit()
        


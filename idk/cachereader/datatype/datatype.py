from functools import cmp_to_key
from collections import namedtuple

from common.deco import cachedprop, singleton
from ..base import BinaryElementBase
from ..location import UndefinedLocation

class DataTypeBase(BinaryElementBase):
    def __init__(self, elf, name, loc_id):
        BinaryElementBase.__init__(self, elf)
        self.name = name
        self.loc_id = loc_id

    @property
    def location(self):
        if self.loc_id is not None:
            return self.elf.locations[self.loc_id]
        else:
            return UndefinedLocation

    @property
    def is_anonymous(self):
        '''True if the element is anonymous.'''
        return not self.name

    @property
    def safename(self):
        '''Name of the datatype or '<anonymous>' if anonymous.'''
        return self.name if not self.is_anonymous else '<anonymous>'


class TypeInfo(DataTypeBase):
    def __init__(self, elf, parent_id, name, loc_id):
        DataTypeBase.__init__(self, elf, name, loc_id)
        self.parent_id = parent_id

    @property 
    def parent(self):
        return self.elf.types[self.parent_id]


class DataType(DataTypeBase):

    def __init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id):
        DataTypeBase.__init__(self, elf, name, loc_id)

        self.tid = tid 
        self.kind = kind
        self.byte_size = byte_size
        self.inner_id = inner_id 

    def _get_is_scalar(self):
        return False

    def _get_printf_pattern(self):
        return '<dumping of %s is not supported>' % self

    @property 
    def printf_pattern(self):
        return self._get_printf_pattern()

    def get_printf_elements(self, name):
        return []

    def simple_printf(self, name):
        return '%s = ' + self.printf_pattern, ['"%s"' % name] + self.get_printf_elements(name)

    def simple_adbi_printf(self, name):
        fmt, args = self.simple_printf(name)
        if args < 1:
            return 'adbi_printf("' + fmt + '");'
        else:
            return 'adbi_printf("' + fmt + '", %s);' % ', '.join(args)

    @property
    def is_scalar(self):
        '''True for scalar data types, false for non-scalar data types.

        According to the C language specification, scalar types are types, which hold a single value,
        e.g. int, short, char, void, float and enum types.  All other types are non-scalar, e.g. arrays,
        structs and unions.
        '''
        return self._get_is_scalar()

    @property
    def bit_size(self):
        if self.byte_size is not None:
            return self.byte_size * 8
        else:
            return None

    def get_codename(self):
        if self.is_anonymous:
            return '__adbi_anon_type_%06x' % self.tid
        else:
            return self.name

    def get_innermost_type(self):
        return self

    @property 
    def innermost_type(self):
        return self.get_innermost_type()

    @property
    def codename(self):
        return self.get_codename()
        
    def define(self):
        raise NotImplementedError
    
    def declare(self):
        raise NotImplementedError

    def _define_var_type(self):
        yield self.innermost_type.codename

    def _define_var_name(self, name):
        return name

    @cachedprop
    def is_forwardable(self):
        '''True if the type can be forwarded.'''
        try:
            x = self.declare()
            return True
        except NotImplementedError:
            return False

    def define_var(self, name):
        spec = list(self._define_var_type())
        if name is None:
            spec[-1] = '%s;' % (spec[-1]) 
        else:
            spec[-1] = '%s %s;' % (spec[-1], self._define_var_name(name))
        for line in spec:
            yield line

    def get_soft_requirements(self):
        return set()

    @cachedprop
    def soft_requirements(self):
        '''Set of datatypes, which must be at least declared to define this type.'''
        return self.get_soft_requirements()
    
    def get_hard_requirements(self):
        return set()
    
    @cachedprop
    def hard_requirements(self):
        '''Set of datatypes, which must be already defined to define this type.'''
        return self.get_hard_requirements()  

    @cachedprop
    def requirements(self):
        '''Set of all datatypes, on which this type depends directly.'''
        return self.soft_requirements | self.hard_requirements


    @cachedprop
    def deforder(self):
        '''Return a list of tuples, which describe the datatypes on which this datatype depends.
        
        The types are sorted in the suggested order of definitions.  The first element of each
        tuple is a datatype object.  The second element is a boolean value, which is True 
        if the type should be defined or False if the type only needs to be declared.'''

        TypeDeclaration = namedtuple('TypeDeclaration', 'type is_definition')
        ret = []
        
        def is_declared(datatype):
            return is_defined(datatype) or TypeDeclaration(datatype, False) in ret

        def is_defined(datatype):
            return TypeDeclaration(datatype, True) in ret

        def process_type(datatype):
            if is_defined(datatype):
                # already defined
                return 

            # insert all possible forward declarations
            for each in datatype.requirements:
                if each.is_forwardable and not is_declared(each):
                    ret.append(TypeDeclaration(each, False))

            # if possible, insert a forward declaration of the current type
            if datatype.is_forwardable and not is_declared(datatype):
                ret.append(TypeDeclaration(datatype, False))

            # define all hard requirements
            for each in datatype.hard_requirements:
                process_type(each)

            for each in datatype.soft_requirements:
                if not each.is_forwardable and not is_defined(each):
                    process_type(each)

            # define datatype
            assert all(is_defined(x) for x in datatype.hard_requirements)
            assert all(is_declared(x) for x in datatype.soft_requirements)
            ret.append(TypeDeclaration(datatype, True))

        process_type(self)
        return ret

        
    def __str__(self):
        return self.safename

    def __repr__(self):
        return "<%s tid=%i %s>" % (self.__class__.__name__, self.tid, str(self))


class UnsupportedType(DataType):
    def get_codename(self):
        return 'void /* unsupported */'

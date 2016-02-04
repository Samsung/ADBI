from common.deco import cachedprop
from common.enums import TypeKinds
from .datatype import DataType
from .builtin import VoidType

class NestedType(DataType):
    @property
    def inner(self):
        if self.inner_id is not None:
            return self.elf.types[self.inner_id]
        else:
            return VoidType()

    def get_innermost_type(self):
        return self.inner.get_innermost_type()

    def _define_inner_var(self, name):
        return self.inner._define_var_name(name) if self.inner else name

    def define(self):
        return iter([])

    def _get_is_scalar(self):
        return self.inner.is_scalar

    @property
    def _inner_str(self):
        if self.inner:
            return str(self.inner)
        else:
            return 'void'

class ArrayType(NestedType):
    
    def __init_size(self):
        if self.byte_size is not None:
            return 
        if self.is_unbounded:
            self.byte_size = 4
        else:
            self.byte_size = reduce(lambda x, y: x * y, self.dimensions)
    
    def __init__(self, elf, tid, kind, name, bit_size, inner_id, loc_id):
        NestedType.__init__(self, elf, tid, kind, name, bit_size, inner_id, loc_id)
        self.__init_size()

    def _get_is_scalar(self):
        return False
    
    @cachedprop
    def dimensions(self):
        return list(self.query_db('select size from array_dim where id=? order by num', self.tid))

    @cachedprop
    def is_unbounded(self):
        '''Specifies that the array is unbounded (e.g. int foo[][10]).'''
        return None in self.dimensions

    def _define_var_name(self, name):
        dims = ''.join('[%i]' % i if i is not None else '[]' for i in self.dimensions)
        name = '%s%s' % (name, dims)
        if self.inner: 
            if self.inner.kind == TypeKinds.array and self.kind != TypeKinds.array:
                name = '(%s)' % name

        return self._define_inner_var(name)

    def get_soft_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.soft_requirements
        else:
            return set()
        
    def get_hard_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.hard_requirements
        else:
            return set([self.inner])

    def __str__(self):
        if len(self.dimensions) > 1:
            return '%i-dimensional array of %s' % (len(self.dimensions), self._inner_str)
        else:
            return 'array of %s' % (self._inner_str)
        

class IndirectType(NestedType):
    def __init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id):
        NestedType.__init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id)
        self.byte_size = self.byte_size or 4
        
    def get_soft_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.soft_requirements
        else:
            return set([self.inner])
        
    def get_hard_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.hard_requirements
        else:
            return set()

    def _get_is_scalar(self):
        return True

    def _get_printf_pattern(self):
        return '0x%x'

    def get_printf_elements(self, name):
        return [name];

class PointerType(IndirectType):   
    def _define_var_name(self, name):
        return self._define_inner_var('* %s' % name)

    def __str__(self):
        return 'pointer to %s' % (self._inner_str)

class ReferenceType(IndirectType):
    def _define_var_name(self, name):
        return self._define_inner_var('* /* ref */ %s' % name)

    def __str__(self):
        return 'reference to %s' % (self._inner_str)

class RightReferenceType(IndirectType):
    def _define_var_name(self, name):
        return self._define_inner_var('* /* rref */ %s' % name)

    def __str__(self):
        return 'right reference to %s' % (self._inner_str)

class ModifiedType(NestedType):
    def __init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id):
        NestedType.__init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id)
        self.byte_size = self.byte_size or self.inner.byte_size

    def get_soft_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.soft_requirements
        else:
            return set()
        
    def get_hard_requirements(self):
        if self.inner.is_anonymous:
            return self.inner.hard_requirements
        else:
            return set([self.inner])

class ConstType(ModifiedType):
    def _define_var_name(self, name):
        # in injectables we'll operate on copies of the original objects only, so we can discard
        # the const specifier to avoid trouble when compiling.
        return self._define_inner_var('/* const */ %s' % name)

    def __str__(self):
        return 'const %s' % (self._inner_str)

class VolatileType(ModifiedType):
    def _define_var_name(self, name):
        return self._define_inner_var('volatile %s' % name)

    def __str__(self):
        return 'volatile %s' % (self._inner_str)

class RestrictType(ModifiedType):
    def _define_var_name(self, name):
        return self._define_inner_var('__restrict__ %s' % name)

    def __str__(self):
        return 'restrict %s' % (self._inner_str)

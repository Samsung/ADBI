from .datatype import DataType
from .compound import CompoundType
from .enum import EnumType


class TypedefType(DataType):
    def __init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id):
        DataType.__init__(self, elf, tid, kind, name, byte_size, inner_id, loc_id)
        self.byte_size = self.byte_size or self.inner.byte_size
    
    @property
    def inner(self):
        if self.inner_id is not None:
            return self.elf.types[self.inner_id]
    
    def _get_is_scalar(self):
        # Typedefs are special -- they are simple aliases of other types.  They are scalar 
        # if the inner type is scalar too.
        return self.inner.is_scalar

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
    
    def declare(self):
        if self.inner.is_anonymous:
            raise NotImplementedError
    
        if not isinstance(self.inner, (CompoundType, EnumType)):
            raise NotImplementedError        
        
        return self.define()

    def define(self):
        spec = list(self.inner.define_var(self.name))
        spec[0] = 'typedef ' + spec[0]
        for line in spec:
            yield line

    def _get_printf_pattern(self):
        return self.inner.printf_pattern

    def get_printf_elements(self, name):
        return self.inner.get_printf_elements(name)

    def __str__(self):
        return 'typedef %s' % self.safename
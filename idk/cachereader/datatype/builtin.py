from .datatype import DataType
from common.deco import singleton

class BuiltinType(DataType):
    def define(self):
        yield '// datatype %s does not need to be defined' % self

    def declare(self):
        yield '// datatype %s does not need to be declared' % self

    def _get_is_scalar(self):
        return True

@singleton
class VoidType(BuiltinType):
    def __init__(self):
        DataType.__init__(self, None, None, None, 'void', 0, None, None)

    def _get_is_scalar(self):
        return False

class IntType(BuiltinType):
    def get_codename(self):
        try:
            return {
                1: 'char',
                2: 'short',
                4: 'int',
                8: 'long long int',
                16: '__int128',
            }[self.byte_size] 
        except KeyError:
            raise NotImplementedError

    def _get_printf_pattern(self):
        try:
            return {
                1: '%hh',
                2: '%h',
                4: '%',
                8: '%ll',
            }[self.byte_size] 
        except KeyError:
            raise NotImplementedError

    def get_printf_elements(self, name):
        return [name]

class SignedIntType(IntType):
    def get_codename(self):
        return 'signed %s' % IntType.get_codename(self) 

    def _get_printf_pattern(self):
        return IntType._get_printf_pattern(self) + 'd'

class UnsignedIntType(IntType):
    def get_codename(self):
        return 'unsigned %s' % IntType.get_codename(self)

    def _get_printf_pattern(self):
        return IntType._get_printf_pattern(self) + 'u'

class BooleanType(IntType):
    def _get_printf_pattern(self):
        return IntType._get_printf_pattern(self) + 'd'


class FloatType(BuiltinType):
    def get_codename(self):
        try:
            return {
                2: '__fp16',        # only available on ARM
                4: '__adbi_fp32_t',
                8: '__adbi_fp64_t',
                16: '__adbi_fp128_t',
            }[self.byte_size] 
        except KeyError:
            raise NotImplementedError


class CharType(BuiltinType):
    def get_codename(self):
        try: 
            return {
                1: 'char',
            }[self.byte_size]
        except KeyError:
            raise NotImplementedError

    def _get_printf_pattern(self):
        return '%c'

    def get_printf_elements(self, name):
        return [name]


class SignedCharType(CharType):
    def get_codename(self):
        return 'signed %s' % CharType.get_codename(self)


class UnsignedCharType(CharType):
    def get_codename(self):
        return 'unsigned %s' % CharType.get_codename(self)
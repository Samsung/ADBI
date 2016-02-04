from common.enums import TypeKinds

from .builtin import SignedIntType, UnsignedIntType, SignedCharType, UnsignedCharType, FloatType, BooleanType
from .compound import StructType, ClassType, UnionType
from .datatype import UnsupportedType
from .enum import EnumType
from .indirect import ConstType, PointerType, ReferenceType, RightReferenceType, VolatileType, RestrictType, ArrayType
from .typedef import TypedefType


MAPPING = {
     TypeKinds.int : SignedIntType,
     TypeKinds.uint : UnsignedIntType,
     TypeKinds.char : SignedCharType,
     TypeKinds.uchar : UnsignedCharType,
     TypeKinds.float : FloatType,
     TypeKinds.bool : BooleanType,

     TypeKinds.const : ConstType,
     TypeKinds.ptr : PointerType,
     TypeKinds.ref : ReferenceType,
     TypeKinds.rref : RightReferenceType,
     TypeKinds.volatile : VolatileType,
     TypeKinds.restrict : RestrictType,
     TypeKinds.array : ArrayType,

     TypeKinds.struct: StructType,
     TypeKinds.cls: ClassType,
     TypeKinds.union: UnionType,
     TypeKinds.enum: EnumType,

     TypeKinds.typedef: TypedefType,
}

def create(elf, tid, kind, name, bit_size, inner_id, loc_id):
    cls = MAPPING.get(kind, UnsupportedType)
    return cls(elf, tid, kind, name, bit_size, inner_id, loc_id)

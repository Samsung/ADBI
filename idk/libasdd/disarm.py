#!/usr/bin/python
from ctypes import CDLL
from ctypes import c_char_p, c_uint16, c_uint32
import copy
import os

__libasdd_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'libasdd.so')
__libasdd = CDLL(__libasdd_path)

def __init_fn(name, res, args):
    getattr(__libasdd, name).restype = res
    getattr(__libasdd, name).argtypes = args

__init_fn('disarm', c_char_p, [c_uint32])
__init_fn('disthumb', c_char_p, [c_uint16])
__init_fn('disthumb2', c_char_p, [c_uint32])

__init_fn('disarma', c_char_p, [c_uint32, c_uint32])
__init_fn('disthumba', c_char_p, [c_uint16, c_uint32])
__init_fn('disthumb2a', c_char_p, [c_uint32, c_uint32])

def disarm(u32):
    return copy.deepcopy(__libasdd.disarm(u32))

def disthumb(u16):
    return copy.deepcopy(__libasdd.disthumb(u16))

def disthumb2(u32):
    return copy.deepcopy(__libasdd.disthumb2(u32))

def disarma(u32, pc):
    return copy.deepcopy(__libasdd.disarma(u32, pc))

def disathumba(u16, pc):
    return copy.deepcopy(__libasdd.disthumba(u16, pc))

def disthumb2a(u32, pc):
    return copy.deepcopy(__libasdd.disthumb2a(u32, pc))

def is_thumb2(insn):
    if insn > 0xffff:
        raise TypeError("is_thumb2 tests 16-bit long halfwords")

    return insn >= 0xe800

def thumbcat(first, second):
    return (first << 16) + second

def intify(string):
    res = 0
    for idx, byte in enumerate(string):
        res += ord(byte) << (8 * idx)
    return res

if __name__ == '__main__':
    pass
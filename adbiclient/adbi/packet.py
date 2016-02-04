import struct

class Header(object):

    struct = struct.Struct('<III')
    size = struct.size

    def __init__(self, type, seq, length):
        self.type = type
        self.seq = seq
        self.length = length

    @staticmethod
    def type_encode(name):
        result = 0
        assert len(name) == 4
        for c in name:
            assert ord(c) < 2 ** 8
            result <<= 8
            result |= ord(c)
        return result

    @staticmethod
    def type_decode(num):
        result = []
        for i in xrange(4):
            result.append(chr(num & 0xff))
            num >>= 8
        return ''.join(reversed(result))


    @classmethod
    def unpack_from(cls, data):
        type, seq, length = cls.struct.unpack_from(data)
        return cls(cls.type_decode(type),
                   seq,
                   length)

    def pack(self):
        return self.struct.pack(self.type_encode(self.type),
                                self.seq,
                                self.length)

class Payload(object):

    class Element(object):
        struct = struct.Struct('<BxHI')

        class Type(object):
            TERMINATOR = 0x00
            U32 = 0x10
            I32 = 0x11
            U64 = 0x20
            I64 = 0x21
            STR = 0x80

        def __init__(self, name, type, value):
            self.type = type
            self.name = name
            self.value = value

        @property
        def binvalue(self):
            if self.type == self.Type.TERMINATOR:
                return ''
            elif self.type == self.Type.U64:
                return struct.pack('<Q', self.value)
            elif self.type == self.Type.I64:
                return struct.pack('<q', self.value)
            elif self.type == self.Type.U32:
                return struct.pack('<I', self.value)
            elif self.type == self.Type.I32:
                return struct.pack('<i', self.value)
            elif self.type == self.Type.STR:
                return self.value + '\0'
            else:
                raise ValueError('Invalid data type: {:}.'.format(self.type))

        def pack(self):
            return self.struct.pack(self.type, len(self.name) + 1, len(self.binvalue)) + \
                   self.name + '\0' + self.binvalue

    def __init__(self, elements=None):
        if elements is None:
            self.elements = []
        else:
            self.elements = elements

    def pack(self):
        elements = self.elements + [self.Element('', self.Element.Type.TERMINATOR, None)]
        return ''.join([x.pack() for x in elements])

    def put_u64(self, name, val):
        self.elements.append(self.Element(name,
                                          self.Element.Type.U64,
                                          val))

    def put_i64(self, name, val):
        self.elements.append(self.Element(name,
                                          self.Element.Type.I64,
                                          val))

    def put_u32(self, name, val):
        self.elements.append(self.Element(name,
                                          self.Element.Type.U32,
                                          val))

    def put_i32(self, name, val):
        self.elements.append(self.Element(name,
                                          self.Element.Type.I32,
                                          val))

    def put_str(self, name, val):
        self.elements.append(self.Element(name,
                                          self.Element.Type.STR,
                                          val))

    def get(self, name, default=None):
        for e in self.elements:
            if e.name == name:
                return e.value
        if default is not None:
            return default
        else:
            raise ValueError('No such field: {:}.'.format(name))

    @classmethod
    def unpack_from(cls, data):

        def iter_elements():
            offset = 0
            while True:
                type, namelen, datalen = cls.Element.struct.unpack_from(data,
                                                                        offset)
                offset += cls.Element.struct.size

                name = data[offset:offset + namelen - 1]
                offset += namelen
                value = data[offset:offset + datalen]
                offset += datalen

                if type == cls.Element.Type.I64:
                    value = struct.unpack('<q', value)[0]
                elif type == cls.Element.Type.U64:
                    value = struct.unpack('<Q', value)[0]
                elif type == cls.Element.Type.I32:
                    value = struct.unpack('<i', value)[0]
                elif type == cls.Element.Type.U32:
                    value = struct.unpack('<I', value)[0]
                elif type == cls.Element.Type.STR:
                    value = value[:-1]
                elif type == cls.Element.Type.TERMINATOR:
                    break
                else:
                    raise ValueError('Invalid payload element data type: {:}.'.format(hex(type)))

                yield cls.Element(name, type, value)

        return cls(list(iter_elements()))


class Packet(object):
    def __init__(self, header, payload):
        self.header = header
        self.payload = payload

    @property
    def type(self):
        return self.header.type

    def pack(self):
        return self.header.pack() + self.payload.pack()

    def get(self, name, default=None):
        return self.payload.get(name, default)

    @classmethod
    def unpack_from(cls, data):
        header = Header.unpack_from(data)
        payload = Payload.unpack_from(data[Header.struct.size:])
        return cls(header, payload)
class BaseLEB(object):

    SIGNED = None

    @classmethod
    def decode(cls, stream):
        '''Decode a LEB128 or SLEB128 value from stream and return it. 
        
        The stream must implement the buffer interface.  The method only consumes as much data as required. 
        If there is not enough data to decode a valid number, ValueError is risen.
        
        If signed evaluates to true, a LEB number is decoded.  Otherwise, a SLEB number is decoded.'''
        result = 0
        shift = 0
        while True:
            b = stream.read(1)
            if not b:
                raise ValueError('unexpected end of data')
            b = ord(b)
            result |= (b & 0x7f) << shift
            shift += 7
            if b & 0x80 == 0:
                break
            
        if cls.SIGNED and (b & 0x40):
            result |= -(1 << shift)
        
        return result
    
    @classmethod    
    def encode_iter(cls, value):
        '''Encode the given value as a LEB128 or SLEB128 and yield its bytes.
        
        If signed evaluates to true, a LEB number is encoded.  Otherwise, a SLEB number is encoded.'''
        
        if value < 0 and not cls.SIGNED:
            raise ValueError
        
        while True:
            seven = value & 0x7f
            value >>= 7
            
            if cls.SIGNED:
                last = (value == 0 and not (seven & 0x40)) or ((value == -1) and (seven & 0x40))
            else:
                last = (value == 0) 
            
            yield seven | (0x80 if not last else 0)
            if last:
                break
        
    @classmethod
    def encode(cls, value):
        '''Encode the given value as a LEB128 or SLEB128 and return it as a bytearray.'''
        return bytearray(cls.encode_iter(value))

    
class LEB(BaseLEB):
    '''Class for encoding and decoding LEB128 numbers.'''   
    SIGNED = False


class SLEB(BaseLEB):
    '''Class for encoding and decoding SLEB128 numbers.'''
    SIGNED = True

from rsock import ReliableSocket
from packet import Packet, Header, Payload
from collections import namedtuple
import struct

Symbol = namedtuple('Symbol', 'address name')
Tracepoint = namedtuple('Tracepoint', 'address handler')
Injectable = namedtuple('Injectable', 'id filename refs type name comment')

class ADBIException(Exception):
    pass

class ADBI(object):

    def __init__(self):
        def seqgen():
            while True:
                for seq in xrange(2 ** 32):
                    yield seq

        self.connection = None
        self.seqgen = seqgen()

    def __check_connection(self):
        if not self.connection:
            raise ADBIException('Not connected.')

    def connect(self, address):
        if self.connection:
            raise ADBIException('Already connected.')
        self.connection = ReliableSocket(address)

    def disconnect(self):
        self.__check_connection()
        self.connection.close()
        self.connection = None

    def __recv(self):
        header = Header.unpack_from(self.connection.recv(Header.size))
        payload = self.connection.recv(header.length)
        payload = Payload.unpack_from(payload)
        return Packet(header, payload)

    def __send(self, packet):
        self.connection.send(packet.pack())

    def request(self, type, payload=None):
        self.__check_connection()
        if payload is None:
            payload = Payload()
        header = Header(type,
                        next(self.seqgen),
                        len(payload.pack()))
        packet = Packet(header, payload)
        self.__send(packet)
        response = self.__recv()

        if response.type == 'FAIL':
            raise ADBIException(response.get('msg', 'Request failed.'))
        if response.type == 'USUP':
            raise ADBIException(response.get('msg', 'Not supported.'))
        if response.type == 'MALF':
            raise ADBIException('Protocol error: {:}'.format(response.get('msg', '?')))

        return response

    def ping(self):
        return self.request('PING')

    def quit(self):
        return self.request('QUIT')

    @property
    def executables(self):
        return self.get_text(0)

    def get_memory(self, pid):
        payload = Payload()
        payload.put_u32('pid', pid)
        response = self.request('MAPS', payload)

        def iter_segments():
            for i in xrange(response.get('segc', 0)):
                def get(what):
                    return response.get('seg{:}[{:}]'.format(what, i))
                yield get('lo'), get('hi'), get('type'), get('file'), get('off')
        return sorted(iter_segments())

    def explain_address(self, pid, address):
        payload = Payload()
        payload.put_u32('pid', pid)
        payload.put_u64('address', address)
        return self.request('ADDR', payload)

    def dump(self, pid, address, size):
        payload = Payload()
        payload.put_u32('pid', pid)
        payload.put_u64('address', address)
        payload.put_u32('size', size)
        response = self.request('MEMD', payload)
        words = (response.get('word[%i]' % x) for x in xrange(response.get('size', 0)))
        def tobytes(word):
            a = (word >> 24) & 0xff
            b = (word >> 16) & 0xff
            c = (word >> 8) & 0xff
            d = (word) & 0xff
            return ''.join(chr(x) for x in (d, c, b, a))
        bytes = (tobytes(x) for x in words)
        return ''.join(bytes)

    @property
    def processes(self):
        response = self.request('PROC')
        return set([response.get('procv[{:}]'.format(i)) for i in
                    xrange(response.get('procc', 0))])

    def start(self):
        return self.request('STRT')

    def stop(self):
        return self.request('STOP')

    def ls(self, path):
        payload = Payload()
        payload.put_str('path', path)
        response = self.request('LDIR', payload)
        return set([response.get('entv[{:}]'.format(i)) 
                   + ('/' if response.get('entd[{:}]'.format(i)) else '')
                   for i in xrange(response.get('entc', 0))])

    def loglevel(self, loglevel):
        payload = Payload()
        payload.put_u32('loglevel', loglevel)
        return self.request('LLEV', payload)

    def attach(self, pid):
        payload = Payload()
        payload.put_u32('pid', pid)
        return self.request('ATTC', payload)

    def detach(self, pid):
        payload = Payload()
        payload.put_u32('pid', pid)
        return self.request('DETC', payload)

    def spawn(self, args):
        payload = Payload()
        payload.put_u32('argc', len(args))
        for i, v in enumerate(args):
            payload.put_str('argv[{:}]'.format(i), v)
        return self.request('SPWN', payload)

    def iter_injectable_symbols(self, iid, which):
        if which not in 'EIA':
            raise ValueError
        payload = Payload()
        payload.put_u32('iid', iid)
        response = self.request('INJ' + which, payload)
        for i in xrange(response.get('symc', 0)):
            postfix = '[%i]' % i
            yield Symbol(response.get('symad' + postfix), response.get('symnm' + postfix))

    def get_injectable_imports(self, iid):
        return self.iter_injectable_symbols(iid, 'I')

    def get_injectable_exports(self, iid):  
        return self.iter_injectable_symbols(iid, 'E')

    def get_injectable_adbi(self, iid):
        return self.iter_injectable_symbols(iid, 'A')

    def get_injectable_tracepoints(self, iid):
        payload = Payload()
        payload.put_u32('iid', iid)
        response = self.request('INJT', payload)
        for i in xrange(response.get('tptc', 0)):
            postfix = '[%i]' % i
            yield Tracepoint(response.get('tpta' + postfix), response.get('tpth' + postfix))

    def iter_injectables(self):
        response = self.request('INJQ')
        for i in xrange(response.get('injc', 0)):
            postfix = '[%i]' % i
            yield Injectable(response.get('injid' + postfix),
                             response.get('injfn' + postfix),
                             response.get('injrc' + postfix),
                             response.get('injtp' + postfix),
                             response.get('injnm' + postfix),
                             response.get('injcm' + postfix))

    @property
    def injectables(self):
        return sorted(self.iter_injectables())

    def injectable_load(self, path):
        payload = Payload()
        payload.put_str('path', path)
        return self.request('INJL', payload)

    def injectable_unload(self, iid):
        payload = Payload()
        payload.put_u32('iid', iid)
        return self.request('INJU', payload)

    def kill(self, pid):
        payload = Payload()
        payload.put_u32('pid', pid)
        return self.request('KILL', payload)
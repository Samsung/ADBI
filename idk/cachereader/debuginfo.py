import sqlite3
import os.path

from .cfa import CallFrameAddress
from .files import Files
from .framepointers import Framepointers
from .function import Functions
from .insnset import InsnSet
from .lines import Lines
from .location import Locations
from .types import Types
from .variables import Variables
from .symbols import Symbols

from cachebuilder import DebugInfo as DebugInfoWriter


class DebugInfo:
    def __init__(self, path, conn):
        self.conn = conn
        self.path = path

        self.cfa = CallFrameAddress(self)
        self.files = Files(self)
        self.framepointers = Framepointers(self)
        self.functions = Functions(self)
        self.insnset = InsnSet(self)
        self.lines = Lines(self)
        self.locations = Locations(self)
        self.types = Types(self)
        self.variables = Variables(self)
        self.symbols = Symbols(self)

    @classmethod
    def loadcached(cls, path, dbpath=None):
        '''Load a new cache for the given file.'''
        dbpath = dbpath or path + '.ac'

        def get_file_time(path):
            '''Get the modification time of the given file.'''
            try:   
                return os.path.getmtime(path)
            except OSError:
                return 0

        if not os.path.isfile(path):
            raise IOError('Binary file does not exist: %s.' % path)

        if not os.path.isfile(dbpath):
            raise ValueError('No cache file exists for %s.' % path)

        if get_file_time(dbpath) < get_file_time(path):
            raise ValueError('Cache older than binary.')

        return cls(path, sqlite3.connect(dbpath))

    @classmethod
    def load(cls, path, dbpath=None, store=True):
        '''Load or create a debug cache for the given file.'''
        try:
            return cls.loadcached(path, dbpath)
        except ValueError:
            with open(path, 'rb') as elf:
                writer = DebugInfoWriter(elf)
                if store:
                    writer.store(dbpath)
                return cls(path, writer.cache)


    def get_CFA_expression(self, addr):
        return self.cfa[addr]

    def close(self):
        self.conn.close()

    def query_db(self, query, *args):
        '''Query the database and yield rows as tuples or single objects.'''
        for e in self.conn.execute(query, tuple(args)):
            if len(e) == 1:
                yield e[0]
            else:
                yield e

    def query_db_one(self, query, *args):
        '''Query the database and return one matching row as tuple or single object.'''
        for e in self.conn.execute(query, tuple(args)):
            if len(e) == 1:
                return e[0]
            else:
                return e
            break
        return None


    def iter_traceable_lines(self, filename):
        '''Yield line-address pairs of traceable lines in the given file.'''
        query = '''select locations.line, lines.addr 
                    from locations join lines 
                    on locations.id == lines.loc
                    where file=(select id from files where path=?)'''
        return self.query_db(query, filename)
    

    def func2addr(self, filename, fn):
        '''Get function entry address.'''
        if filename:
            filename = self.files.expand(filename)
            query = '''select lo from functions join locations
            on locations.id == functions.loc
            where functions.name = ?
            and locations.file = (select id from files where path = ?)'''
            ret = self.query_db(query, fn, filename)
        else:
            ret = self.query_db('select lo from functions where name = ?', fn)

        ret = set(ret)

        if len(ret) == 1:
            return ret.pop()
        elif ret:
            raise ValueError('ambiguous function name %s. Found at: %s' % (fn, ', '.join([hex(addr) for addr in ret])))
        else:    
            raise ValueError('no such function: %s.' % fn)        

        return ret.pop()

    def sym2addr(self, name, symbol_type=None):
        '''Get symbol entry address'''
        if symbol_type:
            ret = self.query_db('select value from symbols where name = ? and type = ?', name, symbol_type)
        else:
            ret = self.query_db('select value from symbols where name = ?', name)

        ret = set(ret)

        if len(ret) == 1:
            return ret.pop()
        elif ret:
            raise ValueError('multiple symbols with name %s. addresses: %s' % (name, ', '.join([hex(value) for value in ret])))
        else:
            raise ValueError('no such symbol: %s.' % name)

        return ret.pop()


    def line2addr(self, path, line):
        
        path = self.files.expand(path)
        query = '''select lines.addr from lines join locations
        on lines.loc == locations.id
        where locations.line = ?
        and locations.file = (select id from files where path = ?)'''
        ret = self.query_db(query, line, path)
    
        ret = list(ret)

        if len(ret) == 1:
            return ret.pop()
        elif ret:
            raise ValueError('location ambiguous: %s:%i.' % (self.files.simplify(path), line))
        else:
            raise ValueError('location invalid or not traceable: %s:%i.' % (self.files.simplify(path), line))
        
        
    def get_addr(self, spec, use_symbols=False):
        
        spec = spec.strip()
        
        if spec.startswith('*'):
            return int(spec[1:], 0)
        
        colon_idx = spec.rfind(':')

        if colon_idx == -1:
            offset = 0
            offset_idx = spec.rfind('+')
            if offset_idx > -1:
                offset = int(spec[offset_idx + 1:], 16)
                spec = spec[:offset_idx]

            # function
            func = spec.strip()
            if use_symbols:
                return self.sym2addr(func, 'STT_FUNC') + offset
            else:
                return self.func2addr(None, func) + offset
        else:
            filename = spec[:colon_idx]
            linefunc = spec[colon_idx + 1:]        
            try:
                line = int(linefunc)
            except ValueError:
                func = linefunc.strip()
                return self.func2addr(filename, func)
            
            return self.line2addr(filename, line)


    def get_datatype(self, spec):
        pass

    def iter_vars(self, address):
        return (x[0] for x in self.conn.execute('select name from addr2vars where lo <= ? < hi', (address,)))

    def iter_locals(self, address):
        idx = self.addr2func_id(address)
        if not idx:
            return
        return (x[0] for x in self.conn.execute('select distinct name from vars2func join vars on vars2func.var = vars.id where func = ?', (idx,)))

    def addr2sym_id(self, address):
        return self.conn.execute('select id from symbols where value <= ? and ? < value + size', (address, address,))

    def addr2func(self, address):
        ret = self.conn.execute('select func from addr2func where lo <= ? < hi', (address,))
        if ret:
            return ret[0]

    def addr2func_id(self, address):
        ret = self.conn.execute('select id from addr2func where lo <= ? < hi', (address,)).fetchone()
        if ret:
            return ret[0]

    def get_func_range(self, address):
        ret = self.conn.execute('select lo, hi from addr2func where lo <= ? < hi', (address,)).fetchone()
        return ret

from contextlib import closing
import os
import sqlite3

from elftools.elf.constants import SH_FLAGS
from elftools.elf.elffile import ELFFile

from common.deco import cachedprop

from .cfi import CallFrameInfo
from .datatype import DataTypes
from .functions import Functions
from .insnset import InsnSet
from .lines import Lines
from .symbols import Symbols
from .location import Locations
from .variables import Variables

SCHEMA = open(os.path.join(os.path.dirname(__file__), 'schema.sql')).read()

class DebugInfo(object):

    def __init__(self, fileobject):
        self.file = fileobject
        self.path = fileobject.name
        self.elf = ELFFile(self.file)

        if not self.elf.has_dwarf_info():
            raise ValueError('No DWARF information present!')

        self.dwarf = self.elf.get_dwarf_info()
        self.sections = [s for s in self.elf.iter_sections()]
        self.symbols = Symbols(self)
        
        self.locations = Locations(self)
        self.lines = Lines(self)
        self.types = DataTypes(self)
        self.variables = Variables(self)
        self.functions = Functions(self)
        self.cfi = CallFrameInfo(self)

        self.insnset = InsnSet(self)

    def addr2fo(self, addr):
        '''Convert given virtual address to file offset.'''
        for section in [s for s in self.sections if s.header['sh_flags'] & SH_FLAGS.SHF_ALLOC]:
            lo = section.header['sh_addr']
            hi = lo + section.header['sh_size']
            if lo <= addr <= hi:
                offset = addr - lo
                return section.header['sh_offset'] + offset
        raise ValueError('Address %x is invalid.' % addr)


    def store(self, dbpath=None):
        '''Store the database in a file on disk.'''

        # Set the path
        dbpath = dbpath or self.path + '.ac'

        # Delete any old database
        try:
            os.remove(dbpath)
        except OSError:
            pass

        # Write the new database directly by executing the dump from cache.
        with closing(sqlite3.connect(dbpath)) as conn:
            script = '\n'.join(self.cache.iterdump())
            conn.executescript(script)


    @cachedprop 
    def cache(self):
        '''Connection to read-only database, which represents the database.'''
        conn = sqlite3.connect(':memory:')
        
        # Apply schema
        conn.executescript(SCHEMA)
            
        # Store tables
        self.locations.store(conn)
        self.lines.store(conn)
        self.types.store(conn)
        self.variables.store(conn)
        self.functions.store(conn)
        self.cfi.store(conn)
        self.symbols.store(conn)
        self.insnset.store(conn)

        # Each of the methods above should commit, commit again just in case.
        conn.commit()

        # TODO: Switch database to read-only mode
        return conn


from collections import namedtuple
from elftools.elf.sections import SymbolTableSection
import itertools
import logging


Symbol = namedtuple('Symbol', 'id name value size bind type visibility shndx')

class Symbols(object):
    def __init__(self, debuginfo):
        self.debuginfo = debuginfo
        self.id_gen = itertools.count()

        # Symbol sections
        symsect_iter = (section 
                        for section in debuginfo.elf.iter_sections() 
                        if isinstance(section, SymbolTableSection))

        # Symbols            
        symbol_iter = (symbol 
                       for section in symsect_iter 
                       for symbol in section.iter_symbols()
                       if not symbol.name.startswith(('$x', '$a', '$t', '$d'))
                       # sometimes linker has all symbols in .symtab beginning with ``__dl_''
                       and not symbol.name.startswith(('__dl_$x', '__dl_$a', '__dl_$t', '__dl_$d')))

        def iter_symbols():
            for symbol in symbol_iter:
                e = symbol.entry
                yield Symbol(self.id_gen.next(), symbol.name,
                             e.st_value, e.st_size, e.st_info.bind, e.st_info.type, e.st_other.visibility, e.st_shndx)
 
        self.symbols = list(iter_symbols())

    def store(self, conn):
        logging.debug('Storing %i symbols' % len(self.symbols))
        query = 'insert into symbols(id, name, value, size, bind, type, vis, shndx) values (?, ?, ?, ?, ?, ?, ?, ?)'
        items = ((symbol.id, symbol.name, symbol.value, symbol.size, symbol.bind, symbol.type, symbol.visibility, symbol.shndx) 
                 for symbol in self.symbols)
        conn.executemany(query, items)
        conn.commit()
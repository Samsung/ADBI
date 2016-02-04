from elftools.elf.sections import SymbolTableSection

from common.enums import InsnKinds

class InsnSet:
    def __init__(self, debug_info):
        
        def get_kind(sym):
            if sym.name.startswith(('$x', '__dl_$x')):
                return InsnKinds.arm64
            elif sym.name.startswith(('$a', '__dl_$a')):
                return InsnKinds.arm
            elif sym.name.startswith(('$t', '__dl_$t')):
                return InsnKinds.thumb
            else:
                return InsnKinds.nocode

        # Symbol sections
        symsect_iter = (section 
                        for section in debug_info.elf.iter_sections() 
                        if isinstance(section, SymbolTableSection))

        # Symbols            
        symbol_iter = (symbol 
                       for section in symsect_iter 
                       for symbol in section.iter_symbols())
            
        # Mapping symbols    
        mapsymbol_iter = (symbol 
                          for symbol in symbol_iter
                          if symbol.name.startswith(('$x', '$a', '$t', '$d'))
                          # sometimes linker has all symbols in .symtab beginning with ``__dl_''
                          or symbol.name.startswith(('__dl_$x', '__dl_$a', '__dl_$t', '__dl_$d')))

        def iter_mapping():
            for symbol in mapsymbol_iter:
                try:
                    addr = debug_info.addr2fo(symbol.entry.st_value)
                    kind = get_kind(symbol)
                    yield addr, kind
                except ValueError:
                    pass

        self.mapping = list(iter_mapping())

        
    def store(self, conn):
        conn.executemany('insert into insnset(addr, kind) values (?, ?)', self.mapping)
        conn.commit()


            
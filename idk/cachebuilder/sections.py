from collections import namedtuple
import logging
from elftools.elf.constants import SH_FLAGS

Section = namedtuple('Section', 'id name type addr offset size flags')

class Sections(object):
    def __init__(self, debuginfo):
        self.debuginfo = debuginfo
 
        def iter_sections():
            # omit first section - it is always null section
            for idx in range(1, self.debuginfo.elf.num_sections()):
                section = self.debuginfo.elf.get_section(idx)
                h = section.header
                yield Section(idx, section.name, h['sh_type'], h['sh_addr'], h['sh_offset'], h['sh_size'], h['sh_flags'])
        
        self.sections = list(iter_sections())

    def addr2fo(self, addr):
        '''Convert given virtual address to file offset.'''
        for section in [s for s in self.sections if s.flags & SH_FLAGS.SHF_ALLOC]:
            lo = section.addr
            hi = lo + section.size
            if lo <= addr < hi:
                offset = addr - lo
                return section.offset + offset
        raise ValueError('Address %x is invalid.' % addr)

    def store(self, conn):
        logging.debug('Storing ELF sections')
        query = 'insert into sections(id, name, type, addr, offset, size, flags) values (?, ?, ?, ?, ?, ?, ?)'
        items = ((section.id, section.name, section.type, section.addr, section.offset, section.size, section.flags) 
                 for section in self.sections )
        conn.executemany(query, items)
        conn.commit()
from base64 import b64encode
from collections import namedtuple
import logging

from elftools.dwarf.callframe import FDE

from common.leb128 import LEB, SLEB 

CallFrameInfoEntry = namedtuple('CallFrameInfoEntry', 'low high expr')

class CallFrameInfo:
    def __init__(self, debug_info):
        self.debug_info = debug_info
        
        def iter_entries():
            if not self.debug_info.dwarf.has_CFI():
                logging.warn('ELF has no call frame information.')
                return 

            for fde in self.debug_info.dwarf.CFI_entries():
                
                if not isinstance(fde, FDE):
                    continue
                
                low = fde.header.initial_location
                high = low + fde.header.address_range
                            
                invalid = False
                decoded = fde.get_decoded().table
                for n, each in enumerate(decoded, 0):
                    try:
                        entry_low = debug_info.addr2fo(each['pc'])
                        entry_high = debug_info.addr2fo(high if n >= len(decoded) - 1 else decoded[n + 1]['pc'])
                    except ValueError:
                        invalid = True
                        continue

                    cfa_rule = each['cfa']
                    if cfa_rule.expr is not None:
                        # CFA is a regular DWARF expressions
                        expr = cfa_rule.expr
                    else:
                        assert cfa_rule.reg is not None
                        # CFA is a register + offset -- convert it to a DWARF expression
                        expr = bytearray([0x92])                # DW_OP_bregx 
                        expr += LEB.encode(cfa_rule.reg)        # Register index
                        expr += SLEB.encode(cfa_rule.offset)    # Offset from register
                    yield CallFrameInfoEntry(entry_low, entry_high, b64encode(expr)) 

                if invalid:
                    logging.warn('Invalid call frame information entry encountered in FDE at %#x.', fde.offset)
            
        self.entries = list(iter_entries())
     
    def store(self, conn):
        logging.debug('Storing %i call frame information entries.', len(self.entries))
        query = '''insert into cfi (lo, hi, expr) values (?, ?, ?)'''
        conn.executemany(query, self.entries)
        conn.commit()
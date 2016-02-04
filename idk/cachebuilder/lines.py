import logging

class Lines:
    def __init__(self, debuginfo):
        def iter_lines():
            for compilation_unit in debuginfo.dwarf.iter_CUs():
                lineprog = debuginfo.dwarf.line_program_for_CU(compilation_unit)
                for entry in lineprog.get_entries():
                    if (entry.state is None) or (entry.state.end_sequence):
                        continue

                    if not entry.state.is_stmt:
                        continue

                    filename = debuginfo.locations.get_source_file(compilation_unit, entry.state.file)
                    line = entry.state.line
                    col = entry.state.column
                    try:
                        addr = debuginfo.addr2fo(entry.state.address)
                    except ValueError:
                        # line (probably) optimized out
                        logging.warn('Location %s:%i:%i is mapped to an invalid address %#x (optimized out?).', 
                                     filename, line, col, entry.state.address)
                        addr = None

                    loc_id = debuginfo.locations.insert_flc(filename, line, col)

                    yield addr, loc_id

        self.lines = list(iter_lines())


    def store(self, conn):
        logging.debug('Storing %i line to address mapping entries.', len(self.lines))

        query = 'insert into lines values (?, ?)'
        items = ((addr, loc_id) for addr, loc_id in self.lines)
        conn.executemany(query, items)

        conn.commit()
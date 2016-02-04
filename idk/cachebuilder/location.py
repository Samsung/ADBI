from itertools import count
import logging
import os


def iter_files(compilation_unit):
    '''Yield all file paths in the given compilation unit.

    Yield absolute paths if possible.  Paths are not guaranteed to be unique.'''
    
    topdie = compilation_unit.get_top_DIE()

    def iter_files_raw():
        # Yield base file (main source file, given as parameter to C compiler)
        yield topdie.attributes['DW_AT_name'].value

        # Get the DWARF line program
        lineprog = compilation_unit.dwarfinfo.line_program_for_CU(compilation_unit)

        # Yield other files (usually headers)
        for file_entry in lineprog.header.file_entry:
            filename = file_entry.name

            # Add directory prefix
            dir_idx = file_entry.dir_index
            if dir_idx:
                directory = lineprog.header.include_directory[dir_idx - 1]
                filename = os.path.join(directory, filename)

            yield filename

    try:
        basedir = topdie.attributes['DW_AT_comp_dir'].value
    except KeyError:
        # Base directory can not be 
        logging.warn('compilation unit %s uses relative file paths', topdie.attributes['DW_AT_name'].value)
        basedir = '.'

    # yield postprocessed paths
    return (os.path.normpath(os.path.join(basedir, x)) for x in iter_files_raw())


class Locations(object):

    def __init__(self, debuginfo):
        self.debuginfo = debuginfo
        self.locations = {}
        self.id_gen = count()

        # Map compilation units to the list of their paths  
        self.cufiles = { compilation_unit.cu_offset : list(iter_files(compilation_unit)) 
                        for compilation_unit in debuginfo.dwarf.iter_CUs() }

        # Map file paths to their IDs, don't store duplicates
        files = sorted(set(sum(self.cufiles.itervalues(), [])))
        self.files = { path: idx for idx, path in enumerate(files) } 
            
        for each in self.files:
            if not os.path.isfile(each):
                logging.warn('source file does not exist: %s', each)


    def get_source_file(self, compilation_unit, idx):
        '''Get the path of the source file with the given ID from the compilation unit.

        This method uses the original symbol indexes from the DWARF file.  The returned paths are normalized and 
        converted to absolute (if possible).

        The first parameter, compilation_unit, is the compilation unit object supplied by elftools.'''
        return self.cufiles[compilation_unit.cu_offset][idx]


    def insert_flc(self, filename, line, column):
        '''Insert a new location (file-line-column) and return its ID in the database.

        If the same location already exists in the database, the ID of the existing location is returned and no new 
        records are created.''' 
        flc = (self.files[filename], line, column)
        try:
            ret = self.locations[flc] 
        except KeyError:
            ret = self.id_gen.next()
            self.locations[flc] = ret
        
        return ret


    def insert_DIE_flc(self, die):
        '''Insert a new location basing on the location attributes of the given DIE and return its ID in the database.

        Internally, this method uses insert_flc.'''
        def get_attr_val(what):
            try:
                return die.attributes[what].value
            except KeyError:
                return None

        file_idx = get_attr_val('DW_AT_decl_file')
        if file_idx is None:
            return None
        
        line = get_attr_val('DW_AT_decl_line') or 0
        column = get_attr_val('DW_AT_decl_column') or 0
        filename = self.get_source_file(die.cu, file_idx)

        return self.insert_flc(filename, line, column)

    def getfile(self, idx):
        if idx is None:
            return None
        for k, v in self.files.iteritems():
            if v == idx:
                return k
        raise KeyError

    def get(self, idx):
        if idx is None:
            return (None, 0, 0)
        for k, v in self.locations.iteritems():
            if v == idx:
                f, l, c = k
                return self.getfile(f), l, c
        raise KeyError

    def getstr(self, idx):
        e = [str(x) for x in self.get(idx) if x] 
        if e:
            return ':'.join(e)
        else:
            return '<unknown location>'

    def store(self, conn):
        logging.debug('Storing %i files and %i locations.', len(self.files), len(self.locations))

        # Store files
        query = 'insert into files (path, id) values (?, ?)'
        items = self.files.iteritems() 
        conn.executemany(query, items)
        conn.commit()

        # Store locations
        query = 'insert into locations (id, file, line, col) values (?, ?, ?, ?)'
        items = ((lid, file_id, line, col) for (file_id, line, col), lid in self.locations.iteritems())
        conn.executemany(query, items)
        conn.commit()


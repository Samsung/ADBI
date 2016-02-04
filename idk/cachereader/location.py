from base import BinaryElementBase


class Location(BinaryElementBase):
    def __init__(self, elf, file, line, col):
        BinaryElementBase.__init__(self, elf)
        self.trio = (file, line, col)

    @property
    def file(self):
        return self.trio[0]

    @property
    def sfile(self):
        if self.file:
            return self.elf.files.simplify(self.file)

    @property
    def line(self):
        return self.trio[1]

    @property
    def col(self):
        return self.trio[2]

    def __hash__(self):
        return hash(self.file) ^ self.line ^ self.col

    def __cmp__(self, other):
        return cmp(self.trio, other.trio)

    def __getitem__(self, idx):
        return self.trio[idx]

    def __str__(self):
        coords = [str(x) for x in (self.sfile, self.line, self.col) if x]
        if coords:
            return ':'.join(coords)
        else:
            return '???'

UndefinedLocation = Location(None, None, None, None)


class Locations(BinaryElementBase):
    def __getitem__(self, idx):
        query = '''select path, line, col
        from locations join files
        on files.id = locations.file
        where locations.id = ?
        limit 1'''
        q = self.query_db(query, idx)
        for file, line, col in q:
            return Location(self.elf, file, line, col)
        raise KeyError
    

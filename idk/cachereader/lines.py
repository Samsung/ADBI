from base import BinaryElementBase

class Lines(BinaryElementBase):
    def __init__(self, elf):
        BinaryElementBase.__init__(self, elf)

    def iter_locations(self, low, high):
        '''Yield address-location pairs in the given address range.'''
        query = '''select addr, loc from lines where ? <= addr and addr < ? order by addr'''
        for addr, lid in self.query_db(query, low, high):
            yield addr, self.elf.locations[lid]

    def get_location(self, addr):
        '''Get the location mapped to the given address.'''
        query = 'select loc from lines where addr = ?'
        return self.elf.locations[self.query_db(query, addr)]


from cachereader.base import BinaryElementBase
from common.deco import cachedfn


class Symbol(BinaryElementBase):
    def __init__(self, elf, idx, name, value, size, bind, stype, vis, shndx):
        BinaryElementBase.__init__(self, elf)
        self.idx = idx
        self.name = name
        self.value = value
        self.size = size
        self.bind = bind
        self.type = stype
        self.visibility = vis
        self.shndx = shndx

class Symbols(BinaryElementBase):

    @cachedfn
    def __getitem__(self, idx):
        query = 'select name, value, size, bind, type, vis, shndx from symbols where id = ?'
        name, value, size, bind, stype, vis, shndx = self.query_db_one(query, idx)
        return Symbol(self.elf, idx, name, value, size, bind, stype, vis, shndx)

    def __iter__(self):
        query = 'select id from symbols'
        for idx in self.query_db(query):
            yield self[idx]
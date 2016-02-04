from base import BinaryElementBase
from common.deco import cachedfn, cachedprop
import datatype


class Types(BinaryElementBase):

    def __init__(self, elf):
        BinaryElementBase.__init__(self, elf)

    @cachedfn
    def __getitem__(self, idx):
        try:
            query = 'select kind, name, bytes, inner, loc from types where id=?'
            kind, name, byte_size, inner_id, loc_id = self.query_db_one(query, idx)
            return datatype.create(self.elf, idx, kind, name, byte_size, inner_id, loc_id)
        except ValueError:
            raise KeyError

    @cachedprop
    def names(self):
        return set([str(each) for each in self if not each.is_anonymous])

    def __iter__(self):
        for tid in self.query_db('select id from types'):
            yield self[tid]

    def get_by_name(self, name):
        return set(each for each in self if each.name == name or str(each) == name)
        
        
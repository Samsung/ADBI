from .base import BinaryElementBase

from common.deco import rangecachedfn
from common.enums import InsnKinds


class InsnSet(BinaryElementBase):
    def __init__(self, elf):
        BinaryElementBase.__init__(self, elf)

    @rangecachedfn
    def get_kind_range(self, addr):
        res = self.query_db_one('select kind, addr from insnset where addr <= ? order by addr desc limit 1', addr)
        hi = self.query_db_one('select addr from insnset where ? < addr order by addr asc limit 1', addr)

        kind = res[0] if res else InsnKinds.nocode
        lo = res[1] if res else 0
        hi = hi if hi else addr + 32

        return kind, lo, hi

    def get_kind(self, addr):
        res, _, _ = self.get_kind_range(addr)
        return res

    def __getitem__(self, idx):
        return self.get_kind(idx)
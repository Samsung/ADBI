class BinaryElementBase(object):
    def __init__(self, elf):
        self.elf = elf
        if self.elf:
            self.query_db = elf.query_db
            self.query_db_one = elf.query_db_one 
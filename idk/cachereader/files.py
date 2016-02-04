import os.path

from base import BinaryElementBase


class Files(BinaryElementBase):
    
    def __init__(self, elf):
        BinaryElementBase.__init__(self, elf)
        self.files = set(self.query_db('select distinct path from files'))

        self.prefix = os.path.commonprefix([x for x in self.files if not x.startswith('/tmp/')])
        idx = self.prefix.rfind('/')
        if idx > 0:
            self.prefix = self.prefix[:idx + 1]
        else:
            self.prefix = ''
                        
        def unique_end(path):
            conflicts = set(each for each in self.files if each != path)
                
            slashpos = None                     
            while True:
                slashpos = path.rfind(os.path.sep, None, slashpos)
                if slashpos == -1:
                    return path
                
                candidate = path[slashpos:]
                conflicts = set(each for each in conflicts if each.endswith(candidate)) 
                if not conflicts:
                    return candidate[1:]

        self.shortfiles = { each:unique_end(each) for each in self.files }

    def __iter__(self):
        return iter(self.files)

    def simplify(self, path):
        if path in self.files:
            return self.shortfiles[path]
        else:        
            if path.startswith(self.prefix):
                return path[len(self.prefix):]
            else:
                return path
        
    def expand(self, path):
        norm = os.path.normpath(path)
        
        if os.path.isabs(norm):
            if norm not in self.files:
                raise ValueError('Unknown source file: %s.' % norm)
            return norm
        
        norm = '/' + norm
        
        matches = [x for x in self.files if x.endswith(norm)]
        
        if len(matches) == 1:
            return matches.pop()
        elif matches:
            raise ValueError('Ambiguous source file: %s.' % path)
        else:
            raise ValueError('No source file matches: %s.' % path)
        
    
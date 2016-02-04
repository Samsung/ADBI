class EqualityMixin(object):
    '''Helper base class for easy equality and inequality implementation. '''

    def _clseq(self, other):
        '''Check if other is an instance of the same class as self.'''
        return isinstance(other, self.__class__)
    
    def __eq__(self, other):
        raise NotImplementedError('__eq__ not implemented in class %s' % self.__class__.__name__)
    
    def __ne__(self, other):
        return not (self == other)
class Result(object):
    '''Dwarf expression result.
    
    Attributes:
    expression -- string representation of evaluated expression (C code)
    is_address -- specifies that the expression represents a memory address
    uses_frame -- specifies that the expression references the function frame base
    uses_cfa   -- specifies that the expression references the call frame address (cfa)'''
    
    def __init__(self, expression, is_address, uses_frame, uses_cfa):
        self.expression = expression 
        self.is_address = is_address
        self.uses_frame = uses_frame 
        self.uses_cfa = uses_cfa 

    def __str__(self):
        return 'DWARF expression result: \"%s\", is address: %s, use frame: %s, use cfa: %s' % (self.expression, self.is_address, self.uses_frame, self.uses_cfa)

    def assign_address(self, typecast=''):
        '''Return a piece of C code, which can be used as a rvalue to assigns the result's address.
        
        The optional typecast parameter can be used to additionally cast the expression to a given type.  In most cases
        the type should be a pointer (eg. some_type *).  If the parameter is omited, the result is not casted and has
        the type (char *).
        
        If the given result has no address (because it is located somewhere else, e.g. in a register), ValueError is 
        risen.
        '''
        if not self.is_address:
            raise ValueError('expression result has no address')
        if typecast:
            return '(%s) (%s)' % (typecast, self.expression)
        else:
            return self.expression
        
    def simple_assign(self, typecast='regval_t'):
        '''Return a piece of C code, which can be used as a rvalue to assign the result to a built-in variable type.
        
        The optional typecast parameter allows to specify the type, to which the result is casted.  The cast is done
        after dereferencing the original address (its type is regval_t).  Note that this method must not be used
        for compound types (e.g. structs).'''
        if self.is_address:
            return '(%s) (*((regval_t *) (%s)))' % (typecast, self.expression)
        else:
            return '(%s) %s' % (typecast, self.expression)
        
    def assign(self, name, scalar=False, typecast=''):
        '''Return a piece of C code, which copies the result value into a variable with the given name.
        
        This method can be used to assign any variable's value correctly.  Note that the value retrieved in the given 
        variable is a copy of the original object -- changes made to this object are not reflected in the original 
        variable.'''
        if self.is_address:
            return '__adbicpy(&%s, %s, sizeof(%s));' % (name, self.expression, name)
        else:
            if scalar:
                if typecast:
                    return '%s = (%s) %s;' % (name, typecast, self.expression)
                else:
                    return '%s = %s;' % (name, self.expression)
            else:
                return '{ unsigned int __adbi_tmp = %s; __adbicpy(&%s, &__adbi_tmp, 4); }' % (self.expression, name)
        
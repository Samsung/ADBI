from base64 import b64encode
from itertools import count
import logging

from elftools.dwarf import constants

from .datatype import TypeReference
from .dwarftools import get_attr_val, iter_ranges, iter_expressions

class Variable(object):
    vid_gen = count()

    def __init__(self, debug_info, die):
        assert die.tag in ('DW_TAG_variable', 'DW_TAG_constant', 'DW_TAG_formal_parameter')
        self.vid = self.vid_gen.next()
        self.name = get_attr_val(die, 'DW_AT_name')
        self.type_id = TypeReference(die, 'DW_AT_type', debug_info.types).resolve().compressed_id
        self.loc_id = debug_info.locations.insert_DIE_flc(die)
        self.is_global = get_attr_val(die, 'DW_AT_external', False) or False

        def iter_var_ranges():
            for low, high in iter_ranges(die):
                try:
                    low = debug_info.addr2fo(low) 
                    high = debug_info.addr2fo(high) 
                    yield low, high
                except (ValueError, TypeError):
                    pass

        self.ranges = list(iter_var_ranges())

        if self.type_id is None:
            raise ValueError('Variable has no type.')

        if self.name is None and die.get_parent().tag not in ('DW_TAG_subprogram'):
            raise ValueError('Variable has no name.')

        def iter_var_expressions():
            optimized_all = True
            optimized_any = False
            for low, high, expr in iter_expressions(die):
                # expr is a list of byte values -- convert to string and encode with base64
                expr = b64encode(''.join(chr(c) for c in expr))

                if low is None:
                    # the expression is independent of the location
                    optimized_all = False
                    yield low, high, expr 
                    continue

                try:
                    low = debug_info.addr2fo(low) 
                    high = debug_info.addr2fo(high) 
                    optimized_all = False
                    yield low, high, expr
                except ValueError:
                    optimized_any = True
                    
            optimized_any = optimized_any or optimized_all
                    
            if optimized_any:
                howstr = 'completely' if optimized_all else 'partially'
                logging.warn('Variable %s declared at %s was %s optimized out.', 
                             self.name, debug_info.locations.getstr(self.loc_id), howstr)

        self.expressions = list(iter_var_expressions())
        if not self.expressions:
            # variable was optimized out, found ranges are not valid
            self.ranges = list()

class Variables(object):

    @staticmethod
    def iter_var_DIEs(debug_info):
        '''Yield all DIEs in the given debug info, which represent variables, constants and parameters.'''
        for cu in debug_info.dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag not in ('DW_TAG_variable', 'DW_TAG_constant', 'DW_TAG_formal_parameter'):
                    continue

                if die.tag == 'DW_TAG_formal_parameter':
                    parent = die.get_parent()
                    if parent:
                        if parent.tag == 'DW_TAG_subroutine_type':
                            # This is not actually a variable, but simply a parameter of a function type. Skip it.
                            # example: typedef void (foo_fn)(int, int);
                            #                                 ^    ^
                            continue
                        if get_attr_val(parent, 'DW_AT_declaration'):
                            # This is the parameter of a function, which is only declared (but not defined).
                            continue
                    
                if get_attr_val(die, 'DW_AT_artificial'):
                    # This is an artificial variable and it has no explicit representation in the source file.  
                    continue

                yield die    

    @staticmethod
    def iter_regular_var_DIEs(debug_info):
        def is_inlined(die):

            if die.tag == 'DW_TAG_compile_unit':
                # we've reached the top DIE
                return False

            if die.tag == 'DW_TAG_subprogram':
                # function
                if 'DW_AT_inline' in die.attributes:
                    # inlined functions -- this is an abstract entry
                    return die.attributes['DW_AT_inline'].value in (constants.DW_INL_inlined, 
                                                                    constants.DW_INL_declared_inlined)
                else:
                    # regular function
                    return False 

            elif die.tag == 'DW_TAG_inlined_subroutine':
                # inlined function instance
                return True

            return is_inlined(die.get_parent())

        return (die for die in Variables.iter_var_DIEs(debug_info) if not is_inlined(die))


    def __init__(self, debug_info):
        self.offset2var = {} 
        for die in self.iter_regular_var_DIEs(debug_info):
            try:
                self.offset2var[die.offset] = Variable(debug_info, die)
            except:
                logging.warning("Variable %s with tag %s is unclear, ignoring.", get_attr_val(die, 'DW_AT_name'), getattr(die, 'tag', "<Unknown>"))
                logging.debug(die)

    def get_by_DIE(self, die):
        '''Return the variable object, which was created for the given DIE.'''
        return self.offset2var[die.offset]
        
    @property
    def variables(self):
        '''List of all known variables.'''
        return self.offset2var.values()

    def store(self, conn):
        # Store variables 
        logging.debug('Storing %i variables.', len(self.variables))
        query = 'insert into variables (id, type, name, global, loc) values (?, ?, ?, ?, ?)'
        items = ((var.vid, var.type_id, var.name, var.is_global, var.loc_id) for var in self.variables)
        conn.executemany(query, items)

        # Store variable ranges
        logging.debug('Storing %i variable ranges.', sum(len(var.ranges) for var in self.variables))
        query = 'insert into variables2ranges (var, lo, hi) values (?, ?, ?)'
        items = ((var.vid, low, high) for var in self.variables for low, high in var.ranges)
        conn.executemany(query, items)

        # Store expressions
        logging.debug('Storing %i variable expressions.', sum(len(var.expressions) for var in self.variables))
        query = 'insert into variables2expressions (var, lo, hi, expr) values (?, ?, ?, ?)'
        items = ((var.vid, low, high, expr) for var in self.variables for low, high, expr in var.expressions)
        conn.executemany(query, items)

        # Commit changes
        conn.commit()

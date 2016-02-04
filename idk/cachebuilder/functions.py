from base64 import b64encode
from collections import namedtuple
import itertools 
import logging

from elftools import dwarf

from .dwarftools import iter_expressions, get_attr_val, get_attr_form_val

FPInfo = namedtuple('FPInfo', 'low high expr')
Function = namedtuple('Function', 'id name low high loc framepointer params')


class Functions:
    @staticmethod
    def iter_params(debug_info, die):
        '''Yield all parameter variables of the function represented by the given DIE.'''
        param_dies = (each for each in die.iter_children() if each.tag == 'DW_TAG_formal_parameter')
        return (debug_info.variables.get_by_DIE(each) for each in param_dies)

    @staticmethod
    def iter_function_DIEs(debug_info):
        '''Yield all DIEs, which represent functions.'''
        for cu in debug_info.dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag == 'DW_TAG_subprogram':
                    if 'DW_AT_inline' in die.attributes:
                        if die.attributes['DW_AT_inline'] in (dwarf.constants.DW_INL_inlined, 
                                                              dwarf.constants.DW_INL_declared_inlined):
                            continue
                    yield die

    def __init__(self, debug_info):
        self.id_gen = itertools.count()
        self.debug_info = debug_info

        
        def iter_functions():
            for die in self.iter_function_DIEs(debug_info):
                try:
                    low = get_attr_val(die, 'DW_AT_low_pc')
                    if not low:
                        # The low pc is NULL. This occurrs when a function is defined in code, but never
                        # accessed.  Skip this entry.
                        continue
                    high_form, high = get_attr_form_val(die, 'DW_AT_high_pc')
                    if high_form.startswith('DW_FORM_data'):
                        high += low
                    name = get_attr_val(die, 'DW_AT_name')
                    name = "<<unnamed function>>" if not name else name

                    def iter_fp():
                        for rlow, rhigh, expr in iter_expressions(die, 'DW_AT_frame_base'):
                            # expr is a list of byte values -- convert to string and encode with base64
                            expr = b64encode(''.join(chr(c) for c in expr))
                            
                            if rlow is None:
                                # Expression is valid everywhere
                                yield FPInfo(rlow, rhigh, expr)
                                continue

                            try:
                                rlow = debug_info.addr2fo(rlow)
                                rhigh = debug_info.addr2fo(rhigh)
                                yield FPInfo(rlow, rhigh, expr)
                                #assert rhigh < high - low
                            except ValueError:
                                logging.warning('Problem with function %s', name)
                                

                    framepointer = list(iter_fp())
                    params = list(self.iter_params(debug_info, die))

                    yield Function(self.id_gen.next(),
                                   name, 
                                   debug_info.addr2fo(low), debug_info.addr2fo(high), 
                                   debug_info.locations.insert_DIE_flc(die),
                                   framepointer, params)

                except KeyError:
                    pass


        self.functions = list(iter_functions())


    def store(self, conn):
        # Store functions
        logging.debug('Storing %i functions.', len(self.functions))
        query = 'insert into functions(id, name, lo, hi, loc) values (?, ?, ?, ?, ?)'
        items = ((function.id, function.name, function.low, function.high, function.loc if function.loc else None) 
                 for function in self.functions)
        conn.executemany(query, items)

        # Store parameters
        logging.debug('Storing function parameters.')
        query = 'insert into params (func, var, idx) values (?, ?, ?)'
        items = ((function.id, var.vid, n) 
                 for function in self.functions
                 for n, var in enumerate(function.params))
        conn.executemany(query, items)        

        # Store frame pointer information
        logging.debug('Storing %i framepointer expressions.', 
                      sum(len(function.framepointer) for function in self.functions))
        query = 'insert into framepointers(func, lo, hi, expr) values (?, ?, ?, ?)'
        items = ((function.id, low, high, expr) 
                 for function in self.functions 
                 for low, high, expr in function.framepointer)
        conn.executemany(query, items)

        # Commit
        conn.commit()
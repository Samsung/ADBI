from elftools.dwarf.ranges import RangeEntry, BaseAddressEntry
from elftools.dwarf import constants

def get_die_offset_by_reference(referer_die, attrname, use_abstract_origin=True):
    '''Return the offset of the DIE referred by the given attribute in the referrer DIE.'''
    ref = referer_die.attributes.get(attrname, None) 

    if attrname != 'DW_AT_abstract_origin' and ref is None and use_abstract_origin:
        origin_die = get_origin_die(referer_die)
        if origin_die:
            return get_die_offset_by_reference(origin_die, attrname, use_abstract_origin)

    if ref is None:
        return None 
    elif ref.form.startswith('DW_FORM_ref'):
        # Reference to a DIE in the current CU
        return referer_die.cu.cu_offset + ref.value
    elif ref.form in ('DW_FORM_ref_sig8', 'DW_FORM_ref_addr'):
        raise NotImplementedError('Type references encoded as %s are not implemented.' % ref.form)
    else:
        raise ValueError


def get_die_by_reference(referer_die, attrname, use_abstract_origin=True):
    '''Return the DIE referred by the given attribute in the referrer DIE.'''

    offset = get_die_offset_by_reference(referer_die, attrname, use_abstract_origin)

    if offset is None:
        return None

    # Iterate through the DIEs searching for the right one 
    for target_die in referer_die.cu.iter_DIEs():
        if target_die.offset == offset:
            return target_die

    # It's not in the current DIE, iterate through all DIEs
    for compilation_unit in referer_die.dwarfinfo.iter_CUs():
        if compilation_unit.cu_offset == referer_die.cu_offset:
            # We've already searched this CU
            continue

        for target_die in compilation_unit.iter_DIEs():
            if target_die.offset == offset:
                return target_die

    raise ValueError


def get_origin_die(die):
    return get_die_by_reference(die, 'DW_AT_abstract_origin')


def get_attr_form_val(die, what, use_abstract_origin=True):
    '''Return the form and value of the given attribute of the given DIE.'''
    try:
        return die.attributes[what].form, die.attributes[what].value
    except KeyError:
        if use_abstract_origin:
            origin_die = get_origin_die(die)
            if origin_die:
                return get_attr_form_val(origin_die, what, use_abstract_origin)
    # Everything failed, no value found
    return None, None


def get_attr_val(die, what, use_abstract_origin=True):
    '''Return the value of the given attribute of the given DIE.'''
    form, val = get_attr_form_val(die, what, use_abstract_origin)
    return val


def iter_ranges(die):
    def iter_range_list(ranges):
        def iter_pairs():
            # by default addresses are relative to the CU base address
            base = die.cu.get_top_DIE().attributes['DW_AT_low_pc'].value
            for entry in ranges:
                if isinstance(entry, BaseAddressEntry):
                    base = entry.base_adress
                elif isinstance(entry, RangeEntry):
                    yield base + entry.begin_offset, base + entry.end_offset
                else:
                    raise ValueError('Invalid element in range list.')

        def merge_ranges(ranges):
            '''Yield ranges equivalent to the given ones, but simplified if possible.'''
            next_range = (None, None)
            for low, high in sorted(ranges):
                if next_range[1] == low:
                    next_range = (next_range[0], high)
                else:
                    if next_range[0] is not None:
                        yield next_range
                
                    next_range = (low, high)

            if next_range[0] is not None:
                yield next_range

        return merge_ranges(iter_pairs())


    if die.tag == 'DW_TAG_subprogram' and 'DW_AT_inline' in die.attributes:
        if die.attributes['DW_AT_inline'].value in (constants.DW_INL_inlined, constants.DW_INL_declared_inlined):
            # inlined function abstract entry 
            return

    # inlined function instance
    if die.tag == 'DW_TAG_inlined_subroutine':
        return

    if 'DW_AT_ranges' in die.attributes:
        rangelist_offset = die.attributes['DW_AT_ranges'].value 
        rl = die.dwarfinfo.range_lists().get_range_list_at_offset(rangelist_offset)
        for low, high in iter_range_list(rl):
            yield low, high
    elif 'DW_AT_low_pc' in die.attributes:
        low = get_attr_val(die, 'DW_AT_low_pc', False)
        high_form, high = get_attr_form_val(die, 'DW_AT_high_pc', False) or low + 1
        if high_form.startswith('DW_FORM_data'):
            high += low
        yield low, high
    elif die.get_parent():
        for x in iter_ranges(die.get_parent()):
            yield x

def iter_loclist(loclistptr):
    raise NotImplementedError

def iter_expressions(die, attr_name='DW_AT_location'):
    def get_loclist(ptr):
        return die.dwarfinfo.location_lists().get_location_list_at_offset(ptr)

    try:
        location_attr = die.attributes[attr_name]
        if location_attr.form == 'DW_FORM_exprloc':
            # Single location expression
            yield None, None, location_attr.value
        elif location_attr.form.startswith('DW_FORM_block'):
            # Single location expression.  This form is not legal for location expressions, 
            # but GCC uses it anyway...
            yield None, None, location_attr.value
        elif location_attr.form == 'DW_FORM_sec_offset':
            for low, high, expr in get_loclist(location_attr.value):
                yield low, high, expr
        elif location_attr.form.startswith('DW_FORM_data'):
            # Another illegal form for location expressions used by GCC.         
            # addresses are relative to cu base address
            cuaddr = die.cu.get_top_DIE().attributes['DW_AT_low_pc'].value
            for low, high, expr in get_loclist(location_attr.value):
                yield cuaddr + low, cuaddr + high, expr
        else:
            raise ValueError('%s form of DW_AT_location is not supported.' % location_attr.form)
    except KeyError:
        pass

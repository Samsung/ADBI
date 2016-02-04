from argparse import ArgumentParser
from glob import fnmatch

from debuginfo import Binary
import output


fnmatch = fnmatch.fnmatchcase



if __name__ == '__main__':
    argp = ArgumentParser()
    argp.add_argument('input', help='input file')
    argp.add_argument('-f', '--functions', action='store_true', help='list functions')
    argp.add_argument('-F', '--function-details', action='store_true', help='list files')
    argp.add_argument('-a', '--address', nargs='*', default=[], help='get address')
    argp.add_argument('-t', '--show-traceable', action='store_true', help='print sourcecode with traceable lines')
    argp.add_argument('-T', '--types', action='store_true', help='print defined types')
    argp.add_argument('-v', '--variables', action='store_true', help='print defined types')
    argp.add_argument('-s', '--scope', type=str, default='', help='report the scope of a variable')

    argp.add_argument('-D', '--define-type', type=str, default='', help='defined given type(s)')

    argp.add_argument('-d', '--show-defs', action='store_true', help='print sourcecode with traceable lines')

    args = argp.parse_args()

    elf = Binary(args.input)



    if args.types:
        print 'Types:'
        def getcodename(t):
            try:
                return t.codename
            except NotImplementedError:
                return ''

        items = (('%x' % type.tid, type.name if type.name else '', str(type), type.byte_size if type.byte_size else 0, type.codename) for type in elf.types)
        items = sorted(items, key=lambda x: int(x[0], 16))
        output.table(items, head=['ID', 'ORIGINAL NAME', 'DESCRIPTION', 'SIZE', 'NAME IN HANDLERS'])

    if args.variables:
        print 'Variables:'
        items = ((var.name, str(var.datatype), str(var.is_global), str(var.location) if var.location else '') 
                  for var in elf.variables)
        output.table(items, head=['NAME', 'TYPE', 'GLOBAL', 'DEFINITION'])        

        for var in elf.variables:
            print var.name
            print var.expressions


    if args.define_type:
        types = (t for t in elf.types if t.name and fnmatch(t.name, args.define_type))
        for t in types:
            print 'Definition of %s (%x):' % (t.safename, t.tid)
            try:
                print '    // Requires:' + ', '.join(str(x) for x in t.required_types)
                lines = list(t.define())
                for line in lines:
                    print '    %s' % line
            except NotImplementedError, e:
                print e
            print


    if args.function_details:
        print 'Function details: '
        functions = sorted(iter(elf.functions), key=lambda x: x.lo)
        table_data = ((fn.name, '%x' % fn.lo, '%x' % fn.hi, fn.location) for fn in functions)
        output.table(table_data, head='FUNCTION LOW HIGH DEFINITION'.split())


    if args.show_traceable:
                
        for filename in elf.files:
            tracemap = sorted((line, addr) for line, addr in elf.iter_traceable_lines(filename))
            lines = sorted(line for line, addr in tracemap)
            
            annotations = {}
            for line in lines:
                addresses = set(addr for tline, addr in tracemap if tline == line)
                variables = [list(elf.variables.iter_locals(addr)) for addr in addresses]
                variables = set(sum(variables, []))
                inaccessible = set(var for var in variables 
                                   if all(var.get_expression(addr) is None 
                                          for addr in addresses))
                variables -= inaccessible
                comment = ', '.join(sorted(str(var) for var in variables))
                if inaccessible:
                    comment += '; inacessible: ' + ', '.join(sorted(str(var) for var in inaccessible))
                annotations[line] = comment
                
            if lines:
                output.annotated_source(filename, marks=lines, annotations=annotations, context=10)
                print

    if args.show_defs:
        functions = sorted(iter(elf.functions), key=lambda x: x.location)

        files = set(fn.location.file for fn in functions if fn.location)

        for filename in files:
            try:
                file_functions = (fn for fn in functions if fn.location.file == filename)
                annotations = dict((fn.location.line, 'function ' + fn.name) for fn in file_functions)
                output.annotated_source(file, annotations=annotations, context=1)
                print
            except IOError:
                pass

    if args.functions:
        print 'Functions: '
        for fn in elf.functions:
            print '    ', fn.name

    for spec in args.address:
        print '    %s \t0x%x' % (spec, elf.get_addr(spec))


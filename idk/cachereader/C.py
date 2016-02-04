import string

INDENT = '    '

KEYWORDS = ['auto',
            'break',
            'case',
            'char',
            'const',
            'continue',
            'default',
            'do',
            'double',
            'else',
            'enum',
            'extern',
            'float',
            'for',
            'goto',
            'if',
            'int',
            'long',
            'register',
            'return',
            'short',
            'signed',
            'sizeof',
            'static',
            'struct',
            'switch',
            'typedef',
            'union',
            'unsigned',
            'void',
            'volatile',
            'while']

ID_FIRST = '_' + string.ascii_letters
ID_OTHER = ID_FIRST + string.digits

CPP_DIRECTIVES = ['assert',
                  'define',
                  'elif',
                  'else',
                  'endif',
                  'error',
                  'ident',
                  'if',
                  'ifdef',
                  'ifndef',
                  'import',
                  'include',
                  'include_next',
                  'line',
                  'pragma',
                  'sccs',
                  'unassert',
                  'undef',
                  'warning']

def is_identifier(s):
    '''Check if the given string represents a valid C identifier.'''
    if not s:
        return False
    if s in KEYWORDS:
        return False
    
    accept = ID_FIRST
    for c in s:
        if c not in accept:
            return False
        accept = ID_OTHER

    return True
    

def to_identifier(s):
    '''Convert the given string to a valid C identifier by replacing invalid chars by an underscore.'''
    if not s:
        return '_'
    if s in KEYWORDS:
        return s + '_'

    def iter_chars():
        accept = ID_FIRST
        for c in s:
            if c in accept:
                yield c
            else: 
                yield '_'
            accept = ID_OTHER

    return ''.join(iter_chars())


def byte_reader(file_object):
    '''Yield bytes from the given file object.'''
    while True:
        data = file_object.read(1)
        if not data:
            break
        yield data
        
        
def strip_comments(iterable, throw_final=True):
    '''Yield bytes Strip C and C++ comments from the given text. 
    
    The iterable argument must contain only byte values (0-255).  The result bytes contain all characters except those 
    enclosed in C or C++ comments.  The only exception is new line characters - those are yield always, even when inside
    a block comment, this way it is easy to determine the correct line number when the result is further processed.
    
    The code is aware of special cases like comment tokens (// or /*) inside literal strings and characters. 
    
    If throw_final evaluates to True, the current state is checked after all input bytes have been processed.  If the 
    internal FSM is not in the final state, a ValueError exception is risen.  This happens only when there are unclosed
    block comments, string or character literals.
    '''
    
    # States
    CODE = 0
    STRING = 1
    STRING_ESCAPE = 2
    CHAR = 3
    CHAR_ESCAPE = 4
    SLASH = 5
    LINECOMMENT = 6
    BLOCKCOMMENT = 7 
    BLOCKASTER = 8
    BLOCKNEWLINE = 9
    
    state = CODE
    
    # State transitions
    transitions = {           
        CODE : {'"': STRING, "'": CHAR, '/': SLASH, },
        STRING : {'"': CODE, '\\': STRING_ESCAPE,},
        STRING_ESCAPE : { '': STRING },
        CHAR : {"'": CODE, '\\': CHAR_ESCAPE},
        CHAR_ESCAPE : {'': CHAR},
        SLASH : {'/': LINECOMMENT, '*': BLOCKCOMMENT, '': CODE},
        LINECOMMENT : {'\n': CODE},
        BLOCKCOMMENT : {'*': BLOCKASTER, '\n': BLOCKNEWLINE},
        BLOCKASTER : { '/': CODE, '*': BLOCKASTER, '': BLOCKCOMMENT, '\n': BLOCKNEWLINE },
        BLOCKNEWLINE : { '\n': BLOCKNEWLINE, '*': BLOCKASTER, '': BLOCKCOMMENT}
    }

    # Output generation (Mealy FSM)
    silent = lambda x : ''
    default = lambda x : x
    transition_out = {
        (CODE, SLASH) : silent,
        (SLASH, CODE) : lambda x: '/' + x,
        (SLASH, BLOCKCOMMENT) : silent,
        (SLASH, LINECOMMENT) : silent,
        (LINECOMMENT, LINECOMMENT) : silent,
        (LINECOMMENT, CODE) : default,
        (BLOCKCOMMENT, BLOCKNEWLINE) : default,
        (BLOCKASTER, BLOCKNEWLINE) : default,
        (BLOCKNEWLINE, BLOCKNEWLINE) : default,
        (BLOCKCOMMENT, None) : silent,
        (BLOCKASTER, None) : silent,
        (BLOCKNEWLINE, None) : silent,
    }
   
    for byte in iterable:
        trans = transitions[state]   
        next = trans.get(byte, trans.get('', state))
        
        trans = (state, next)
        fn = (transition_out.get((state, next), None) or 
              transition_out.get((state, None), None) or 
              transition_out.get((None, next), None) or 
              default)

        out = fn(byte)

        if False:    # Change to True for debugging
            state_desc = {0: 'CODE',
                  1: 'STRING',
                  2: 'STRING_ESCAPE',
                  3: 'CHAR',
                  4: 'CHAR_ESCAPE',
                  5: 'SLASH',
                  6: 'LINECOMMENT',
                  7: 'BLOCKCOMMENT',
                  8: 'BLOCKASTER',
                  9: 'BLOCKNEWLINE'}
            
            out_str = out.replace('\n', '\\n').replace('\t', '\\t')
            print 'FSM %10s -> %10s  :  "%s"' % (state_desc[state], state_desc[next], out_str)

        for c in out:
            yield c
            
        state = next

    # Check for invalid final states

    if not throw_final:
        return

    if state in (STRING, STRING_ESCAPE):
        raise ValueError('''missing terminating '"' character''')
    elif state in (CHAR, CHAR_ESCAPE):
        raise ValueError('''missing terminating ' character''')
    elif state in (BLOCKCOMMENT, BLOCKASTER, BLOCKNEWLINE):
        raise ValueError('''unterminated /* comment''')


def iter_lines(iterable, throw_final=False):
    '''Yield pairs of line number and line contents.
    
    The function takes a file object as parameter and yields pairs of line numbers and their content.  Lines, which 
    were split using the backslash character, are merged and yield together as a single line.  Lines are yield in order,
    but because of merging of split lines, some lines may be missing.
    
    C and C++ comments are automatically removed from the input file using the strip_comments function.  The throw_final
    argument has the same meaning as in the case of the strip_comments function.
    '''
    
    def iter_lines_raw():
        '''Yield whole lines of input characters with the new line character removed.'''
        line = []
        for char in strip_comments(iterable, throw_final):
            if char != '\n':
                line.append(char)
            else:
                yield ''.join(line)
                line = []
        yield ''.join(line)
            
    next_line = []
    lineno = 0
    for lineno, line_raw in enumerate(iter_lines_raw(), 1):
        line_stripped = line_raw.rstrip()
        
        continued = line_stripped.endswith('\\')
        
        if continued:
            line_stripped = line_stripped[:-1]
        
        next_line.append(line_stripped)
        
        if not continued:
            item = ' '.join(next_line)
            yield lineno, item
            next_line = []
        
    item = ' '.join(next_line)
    yield lineno, item



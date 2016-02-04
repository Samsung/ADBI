import sys
import os

INDENT = 4
SEP = 4
LINE = '='

def terminal_width(default=80):
    '''Return the terminal window width or default if not a tty.'''
    if not sys.stdout.isatty():
        return default - 2
    else:
        rows, columns = os.popen('stty size', 'r').read().split()
        return max(int(columns) - 2, 25)


def horizontal_line(text='', width=0, char=LINE):
    '''Print a horizontal line with optional text.'''
    if not width:
        width = terminal_width()

    left = (width - len(text)) / 2
    right = width - left - len(text)

    print char * left + text + char * right


def title(s, char=LINE):
    '''Print text and underline it.'''
    print
    print s
    print char * len(s)


def columnize(elements, width=0, sep=SEP, indent=INDENT):
    '''Print a list dividing it into columns.'''
    if not elements:
        print '<empty>'
        return

    elements = list(str(e) for e in elements)
    maxlen = max(len(e) for e in elements)

    if not width:
        width = terminal_width()

    perline = (width - indent) / (maxlen + sep)
    if not perline:
        perline = 1

    # print lines
    line = ' ' * indent
    for i, e in enumerate(elements):
        line += e
        line += ' ' * (maxlen - len(e) + sep)
        if (i + 1) % perline == 0:
            print line
            line = ' ' * indent

    # last line
    if line.strip():
        print line


def table(data, head=None, align=None, indent=INDENT, sep=SEP):
    '''Print a table.'''
    rows = list(data)
    if not rows:
        return
    if head:
        rows = [head] + rows
    colsc = len(rows[0])
    width = [0] * colsc
    sep = ' ' * sep
    for row in rows:
        for i in xrange(colsc):
            width[i] = max(width[i], len(str(row[i])))
    if align:
        fmt = sep.join('{:%s%i}' % (align[i], width[i])
                         for i in xrange(colsc))
    else:
        fmt = sep.join('{:<%i}' % width[i]
                         for i in xrange(colsc))
    fmt = ' ' * indent + fmt
    for row in rows:
        vals = [str(e) for e in row]
        print fmt.format(*vals)


def human_size(val):
    val = int(val)
    kilo = 2 ** 10
    hold = 9
    suffixes = ' KMGT'
    c = 0
    while val > kilo * hold and c < len(suffixes):
        val /= kilo
        c += 1
    suffix = suffixes[c]
    return (str(val) + ' ' + suffix).strip()


def human_percent(val):
    if val == 0.0:
        return '0 %'

    if val == 1.0:
        return '100 %'

    val = int(val * 100)

    if val == 0:
        return '< 1 %'
    elif val == 100:
        return '~ 100 %'
    else:
        return '%i %%' % val
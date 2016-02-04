import codecs


try:
    from fabulous.color import red, yellow, bold, black
except ImportError:
    identity = lambda x: x
    red = yellow = bold = black = identity


def table(data, head=None, align=None, indent=2, sep=4):
    '''Print a table.'''
    rows = [[str(e) for e in row] for row in data]
    if not rows:
        return

    colsc = len(rows[0])
    sep = ' ' * sep

    if not align:
        align = '>' * colsc

    if head:
        rows = [head] + rows
    width = [max(len(row[column]) for row in rows) for column in xrange(colsc)]

    fmt = (' ' * indent) + sep.join('{:%s%i}' % (align[i], width[i]) for i in xrange(colsc))

    for row in rows:
        vals = [str(e) for e in row]
        print fmt.format(*vals)


def annotated_source(sourcefile, marks=None, annotations=None, context=3, mark='*'):

    marks = marks or []
    annotations = annotations or {}

    interresting = set(marks) | set(annotations)
    printed = [range(n - context, n + context + 1) for n in interresting]
    printed = set(sum(printed, []))

    lastprinted = 0
    with codecs.open(sourcefile, 'r', 'utf-8', 'replace') as f:
        print yellow(sourcefile)
        for n, line in enumerate(f, 1):
            if n in printed:
                m = red(mark if n in marks else ' ' * len(mark))
                c = line.rstrip().expandtabs()
                if n in marks:
                    c = red(c)
                if n in annotations:
                    a = yellow('# ' + annotations[n])
                else:
                    a = ''
                print '  %4i %s   %-80s %s' % (n, m, c, a)
                lastprinted = n
            elif lastprinted == n - 1:
                print bold(black('  %4s' % ('.' * len(str(n)))))
            else:
                pass

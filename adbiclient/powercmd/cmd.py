import difflib
import readline
import inspect

import output

class QuitException(Exception):
    pass

class Cmd(object):

    @staticmethod
    def split(s, b):
        '''Split the given string s by whitespace and return a list of strings.

        Additionally, return the index (in the list) of the word starting
        at index b in the string.
        '''
        def iter_words():
            current_word = ''
            current_word_start = 0
            for i, c in enumerate(s):
                if c.isspace():
                    if current_word:
                        yield current_word_start, current_word
                    current_word = ''
                    current_word_start = i + 1
                else:
                    current_word += c
            if current_word:
                yield current_word_start, current_word

        result_list = []
        result_idx = len(s) + 1
        for start, word in iter_words():
            if b <= start:
                result_idx = min(len(result_list), result_idx)
            result_list.append(word)

        if (s and s[-1].isspace() or not s) and b == len(s):
            result_idx = len(result_list)
            result_list.append('')

        return result_list, result_idx

    def __init__(self):
        readline.set_completer(self.__complete)
        readline.parse_and_bind('tab: complete')
        readline.set_completer_delims(' \t')
        self.candidates = []

    ####################################################################################################################
    ## Sets of commands
    ####################################################################################################################

    @property
    def commands(self):
        '''All available commands.'''
        return set(e[3:].replace('_', ' ') for e in dir(self) if e.startswith('do_'))

    @property
    def documented_commands(self):
        '''Documented commands.'''
        def command_documented(cmd):
            '''Check if cmd is documented.'''
            try:
                return self.__get_help(cmd)[0] != ''
            except AttributeError:
                return False
        return set(c for c in self.commands if command_documented(c))

    @property
    def undocumented_commands(self):
        '''Undocumented commands.'''
        return self.commands - self.documented_commands

    ####################################################################################################################
    ## Completion and conversion helpers
    ####################################################################################################################

    @staticmethod
    def __get_arg_name(fn, argn):
        '''Return the name of the given argument.'''
        argspec = inspect.getargspec(fn)
        assert argspec.args[0] == 'self'
        args = argspec.args[1:]
        if argn < len(args):
            return args[argn]
        elif argspec.varargs:
            return argspec.varargs
        else:
            raise IndexError

    def __complete_arg(self, fn, argn, value):
        '''Convert the given argument using the proper complete_* function.'''
        try:
            argname = self.__get_arg_name(fn, argn)
            complete_fn = getattr(self, 'complete_' + argname)
            argc = len(inspect.getargspec(complete_fn).args)
            assert 1 <= argc <= 2
            try:
                if argc == 1:
                    return complete_fn()
                else:
                    return complete_fn(value)
            except Exception, e:
                print e
        except (AttributeError, IndexError):
            # No such argument or no completion function
            return []

    def __convert_arg(self, fn, argn, value):
        '''Convert the given argument using the proper conv_* function.'''
        try:
            argname = self.__get_arg_name(fn, argn) 
            conv_fn = getattr(self, 'conv_' + argname)
            return conv_fn(value)
        except IndexError:
            # Index out of bounds
            return value
        except AttributeError:
            # No conversion function
            return value

    ####################################################################################################################
    ## Command completion
    ####################################################################################################################

    def __complete(self, text, state):
        '''Internal completion method.'''
        if state == 0:
            try:
                origline = readline.get_line_buffer()
                begin = readline.get_begidx()
                end = readline.get_endidx()
                being_completed = origline[begin:end]
                words, idx = self.split(origline, begin)
                self.candidates = self.complete(words, idx) or []
                self.candidates = [str(x) for x in self.candidates]
                if being_completed:
                    self.candidates = [c for c in self.candidates if c.startswith(being_completed)]
                self.candidates = [c + ' ' if not c.endswith('/') else c for c in self.candidates]
            except Exception, e:
                pass
        try:
            return self.candidates[state]
        except IndexError:
            return None

    def complete(self, words, n):
        '''Public completion method.'''
        try:
            if n == 0:
                return self.commands
            else:
                try:
                    command, args = words[0], words[1:]
                    fn = getattr(self, 'do_' + command)
                    return self.__complete_arg(fn, n - 1, args[n - 1])
                except AttributeError:
                    # No such command, don't make suggestions
                    return []
        except Exception, e:
            pass

    ####################################################################################################################
    ## Command execution
    ####################################################################################################################
    
    def __do_internal(self, fn, givenargs):
        argspec = inspect.getargspec(fn)
        assert argspec.args[0] == 'self'
        args = argspec.args[1:]
                
        maximum = len(args)
        minimum = maximum - len(argspec.defaults or [])
        count = len(givenargs)
        if count < minimum:
            print '*** Too few arguments.'
            return 
        if not argspec.varargs and count > maximum:
            print '*** Too many arguments.'
            return

        new_args = []
        for n, val in enumerate(givenargs):
            try:
                new_args.append(self.__convert_arg(fn, n, val))
            except ValueError, e:   
                print '*** Invalid argument %i:' % (n + 1), e
                return 

        self.run_command(fn, *new_args)


    def do(self, words):
        command, givenargs = words[0], words[1:]
        try:
            fn = getattr(self, 'do_' + command)
            return self.__do_internal(fn, givenargs)
        except AttributeError:
            print '*** No such command:', command
            suggestions = difflib.get_close_matches(command, self.commands)
            if suggestions:
                print '*** Did you mean %s?' % ' or '.join(suggestions)

    ####################################################################################################################
    ## Help commands
    ####################################################################################################################

    def __get_usage(self, cmd):
        '''Return a string describing the usage of the given command.'''
        fn = getattr(self, 'do_' + cmd)

        def iter_args(fn):
            yield cmd
            argspec = inspect.getargspec(fn)
            assert argspec.args[0] == 'self'    
            args = argspec.args[1:]
            defaults = argspec.defaults or []
            for n, arg in enumerate(args):
                if n < len(args) - len(defaults):
                    yield arg.upper()
                else:
                    do = len(args) - len(defaults)
                    di = n - do
                    yield '[%s=%s]' % (arg.upper(), defaults[di])
            if argspec.varargs:
                yield '[%s...]' % argspec.varargs.upper()

        return ' '.join(iter_args(fn))


    def __get_help(self, cmd):
        '''Return a tuple containing the help summary and details.'''
        fn = getattr(self, 'do_' + cmd)
        doc = fn.__doc__.strip()
        if not doc:
            return '', ''
        lines = doc.split('\n')
        return lines[0], '\n'.join(lines[1:])


    def __help_help(self):
        '''Print help summary.'''
        print
        output.title('Commands')
        tab = [[cmd, self.__get_help(cmd)[0]] 
               for cmd in sorted(self.documented_commands)]
        output.table(tab)
        print

        if self.undocumented_commands:
            output.title('Undocumented commands')
            output.columnize(set(self.undocumented_commands))
            print

    def __help_cmd(self, cmd):
        '''Print help for the given command.'''
        try:
            summary, details = self.__get_help(cmd)
            
            if summary.strip():
                print summary.strip()
                print 

            print '    Usage:'
            print '        %s' % self.__get_usage(cmd)

            if details.strip():
                for line in details.split('\n'):
                    print '    %s' % line.strip()

        except AttributeError:
            print '*** No help on %s.' % cmd
            return

    def do_help(self, help_topic=''):
        '''Print help on the given command.'''
        if not help_topic:
            self.__help_help()
        else:
            self.__help_cmd(help_topic)

    def complete_help_topic(self):
        '''Completion for the help command'''
        return self.documented_commands

    ####################################################################################################################
    ## Builtin exit command
    ####################################################################################################################

    def do_exit(self):
        '''
        Exit the program.
        '''
        raise QuitException

    ####################################################################################################################
    ## Other public methods
    ####################################################################################################################

    def interactive(self):
        try:
            while True:
                line = raw_input('# ').split()
                if not line:
                    self.default()
                else:
                    self.do(line)
        except (QuitException, EOFError, KeyboardInterrupt):
            print 'Exiting.'

    def run_command(self, fn, *args):
        '''Run method fn with the given args.'''
        return fn(*args)

    def default(self):
        '''This method gets called when an empty command is entered.'''
        pass


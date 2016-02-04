from functools import wraps


def cachedprop(fn):
    '''Decorator which creates a cached property.''' 

    @wraps(fn)
    def get(self):
        cache_name = '__' + fn.__name__ + '__cache'
        try:
            return self.__dict__[cache_name]
        except KeyError:
            ret = fn(self)
            self.__dict__[cache_name] = ret
            return ret

    return property(get)

def rangecachedfn(fn):
    '''Decorator which creates a range memoized function. Decorator speeds up functions that response
    depends on numeric parameter and is constant in some ranges of this parameter. Decorated function must have
    numeric parameter as second positional parameter. Decorated function must return response, lower boundary
    and high boundary. Response will be cached for all function calls with second parameter in returned range
    and the same other parameters. Keyword arguments are not supported.'''
    memo = {}

    @wraps(fn)
    def wrapper(*args):
        try:
            return memo[args], None, None
        except KeyError:
            rv, lo, hi = fn(*args)
            if hi:
                for i in range(lo, hi):
                    newargs = list(args)
                    newargs[1] = i
                    memo[tuple(newargs)] = rv
            else:
                memo[args] = rv

            return rv, lo, hi

    return wrapper

def cachedfn(fn):
    '''Decorator which creates a memoized function.'''
    memo = {}

    @wraps(fn)
    def wrapper(*args):
        try:
            return memo[args]
        except KeyError:
            #print 'Calling %s(%s)' % (fn.__name__, ', '.join([str(x) for x in args])) 
            rv = fn(*args)
            memo[args] = rv
            return rv

    return wrapper


def singleton(cls):
    '''Convert the given into a singleton.

    This function is ment to be used as a decorator (which should be applied to 
    classes, e.g.:
    
        @singleton
        class Foo:
            pass

    After this, the class can be called (as if an constructor was called), but 
    the call will always return the same instance, e.g.:

        a = Foo()
        b = Foo()
        assert a is b
        assert id(a) == id(b)

    Implementation taken from PEP 318 examples.
    '''
    instances = {}
    def getinstance():
        if cls not in instances:
            instances[cls] = cls()
        return instances[cls]
    return getinstance

    
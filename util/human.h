#ifndef HUMAN_H_
#define HUMAN_H_

#define str_fn(what)                                            \
    struct what ## _t;                                          \
    const char * str_ ## what(const struct what ## _t * what);

str_fn(node)
str_fn(process)
str_fn(thread)
str_fn(injection)
str_fn(injectable)
str_fn(segment)
str_fn(tracepoint)

const char * str_signal(int signo);
const char * str_address(const struct process_t * process, address_t address);

#undef str_fn

#endif

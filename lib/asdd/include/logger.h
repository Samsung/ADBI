#ifndef _LOGGER_H
#define _LOGGER_H

#ifndef __GNUC__
#define __builtin_expect(a, b)  (a)
#endif

#ifndef LOGGER_WIDTH
#define LOGGER_WIDTH    80
#endif

#ifndef LOGGER_INDENT
#define LOGGER_INDENT   4
#endif

#define LOGGER_SILENT   0
#define LOGGER_FATAL    1
#define LOGGER_ERROR    2
#define LOGGER_WARNING  3
#define LOGGER_INFO     4
#define LOGGER_VERBOSE  5
#define LOGGER_DEBUG    6

extern unsigned int logger_level;

void logger_printf(char * fmt, ...) __attribute__((format(printf, 1, 2)));

#define logger_printf_magic(level, ...)                             \
    do {                                                            \
        if (__builtin_expect(!!(level <= logger_level), 0)) {       \
            logger_printf(__VA_ARGS__);                             \
        }                                                           \
    } while (0)

#define critical(...)   do { logger_printf(__VA_ARGS__); } while (0)
#define fatal(...)      logger_printf_magic(LOGGER_FATAL,   __VA_ARGS__)
#define error(...)      logger_printf_magic(LOGGER_ERROR,   __VA_ARGS__)
#define warning(...)    logger_printf_magic(LOGGER_WARNING, __VA_ARGS__)
#define info(...)       logger_printf_magic(LOGGER_INFO,    __VA_ARGS__)
#define verbose(...)    logger_printf_magic(LOGGER_VERBOSE, __VA_ARGS__)

#ifdef NDEBUG
#define debug(...)  do { /* nop */ } while (0)
#else
#define debug(...)  logger_printf_magic(LOGGER_DEBUG, __VA_ARGS__)
#endif

#endif

#ifndef TIME_H_
#define TIME_H_

typedef long time_t;
typedef long suseconds_t;

struct timespec {
    time_t tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};

struct timeval {
    time_t tv_sec;        /* seconds */
    suseconds_t tv_usec;  /* microseconds */
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

#ifdef __aarch64__

SYSCALL_2_ARGS(get_nr(__NR_gettimeofday),
        int, gettimeofday, struct timeval * tv, struct timezone * tz);

SYSCALL_2_ARGS(get_nr(__NR_nanosleep),
        int, nanosleep, const struct timespec * req, struct timespec * rem);

#else

SYSCALL_2_ARGS(get_nr(78), int, gettimeofday, struct timeval * tv, struct timezone * tz);

SYSCALL_2_ARGS(get_nr(162), int, nanosleep, const struct timespec * req, struct timespec * rem);

#endif

/* Simplified variant of the sleep function. */
static void sleep(unsigned int seconds) {
    struct timespec t;
    t.tv_sec  = seconds;
    t.tv_nsec = 0;
    nanosleep(&t, NULL);
}

#endif

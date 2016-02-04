#ifndef NET_H_
#define NET_H_

#include "syscall_template.h"
#include "io.h"

#define SOCK_STREAM      1
#define SOCK_DGRAM       2
#define SOCK_RAW         3
#define SOCK_RDM         4
#define SOCK_SEQPACKET   5
#define SOCK_PACKET      10

enum {
    SHUT_RD = 0,        /* no more receptions */
#define SHUT_RD         SHUT_RD
    SHUT_WR,            /* no more transmissions */
#define SHUT_WR         SHUT_WR
    SHUT_RDWR           /* no more receptions or transmissions */
#define SHUT_RDWR       SHUT_RDWR
};

typedef unsigned short sa_family_t;
typedef int socklen_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct in_addr {
    unsigned int s_addr;
};

#define __SOCK_SIZE__ 16
struct sockaddr_in {
    sa_family_t sin_family;
    unsigned short int sin_port;
    struct in_addr sin_addr;
    
    unsigned char __pad[__SOCK_SIZE__ - sizeof(short int) -
                        sizeof(unsigned short int) - sizeof(struct in_addr)];
};

/* Communication domains */
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10

/* Socket types */
#define SOCK_STREAM      1
#define SOCK_DGRAM       2
#define SOCK_RAW         3
#define SOCK_RDM         4
#define SOCK_SEQPACKET   5
#define SOCK_PACKET      10

/* Special socket options */
#define SOCK_CLOEXEC    O_CLOEXEC
#define SOCK_NONBLOCK   O_NONBLOCK

#ifdef __aarch64__

SYSCALL_3_ARGS(get_nr(__NR_socket),
        int, socket, int domain, int type, int protocol);

SYSCALL_3_ARGS(get_nr(__NR_connect),
        int, connect, int sockfd, const struct sockaddr * addr, socklen_t addrlen);

#else

SYSCALL_3_ARGS(get_nr(281),
        int, socket, int domain, int type, int protocol);

SYSCALL_3_ARGS(get_nr(283),
        int, connect, int sockfd, const struct sockaddr * addr, socklen_t addrlen);

#endif

static inline unsigned int swap32(unsigned int val) {
    return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) | ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}

static inline unsigned short swap16(unsigned short val) {
    return (val >> 8) | (val << 8);
}

static inline unsigned int htonl(unsigned int val) { return swap32(val); }
static inline unsigned int ntohl(unsigned int val) { return swap32(val); }
static inline unsigned short htons(unsigned short val) { return swap16(val); }
static inline unsigned short ntohs(unsigned short val) { return swap16(val); }

static inline unsigned int inet_ip(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
    unsigned int m = 0;
    m |= ((unsigned int) a) << 24;
    m |= ((unsigned int) b) << 16;
    m |= ((unsigned int) c) << 8;
    m |= ((unsigned int) d);
    return htonl(m);
}

#endif /* NET_H_ */

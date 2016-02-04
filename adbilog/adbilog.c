#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 2222

static int tcp_server_sock = -1;

static fd_set sock_set;
static int socket_nfds = 0;

#ifndef DEBUG
#define dbg(...) do { /* nop */ } while (0)
#else
#define dbg(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#endif

static void socket_add_fd(int fd) {
    FD_SET(fd, &sock_set);
    if (fd + 1 > socket_nfds)
        socket_nfds = fd + 1;
}

static void socket_del_fd(int fd) {
    FD_CLR(fd, &sock_set);
    
    for (; fd >= 0; --fd) {
        if (FD_ISSET(fd, &sock_set))
            break;
    }
    
    socket_nfds = fd + 1;
}

static void disconnect(int fd) {
    dbg("disconnecting %i.\n", fd);
    socket_del_fd(fd);
    shutdown(fd, SHUT_RDWR);
    while (close(fd) != 0) {
        assert(errno != EBADF);
    }
}

static bool init() {

    struct sockaddr_in address;
    
    tcp_server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (tcp_server_sock < 0) {
        perror("error creating TCP socket");
        return false;
    }
    
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(tcp_server_sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("error binding TCP socket");
        return false;
    }
    
    if (listen(tcp_server_sock, 1) < 0) {
        perror("error listening on TCP socket");
        return false;
    }
    
    socket_add_fd(tcp_server_sock);
    
    return true;
}

static void cleanup() {
    if (tcp_server_sock >= 0)
        disconnect(tcp_server_sock);
    tcp_server_sock = -1;
}

static void incomming() {
    int client = accept(tcp_server_sock, NULL, NULL);
    if (client == -1) {
        perror("error connecting client");
        return;
    }
    dbg("connected %i.\n", client);
    socket_add_fd(client);
}

static void read_data(int fd) {
    static char buf[1024];
    ssize_t got = recv(fd, buf, 1024, 0);
    
    if (got == 0) {
        /* disconnected */
        disconnect(fd);
    } else {
        write(0, buf, (size_t) got);
    }
}

static void loop() {
    dbg("Entering main loop.");
    while (1) {
        fd_set tmp_set = sock_set;
        int res, fd;
        
        dbg("select (nfds == %i)...\n", socket_nfds);
        res = select(socket_nfds, &tmp_set, NULL, NULL, NULL);
        dbg("select returned %i\n", res);
        
        assert(res >= 0);
        
        for (fd = 0; res > 0; ++fd)
            if (FD_ISSET(fd, &tmp_set)) {
                --res;
                /* fd is readable */
                if (fd == tcp_server_sock) {
                    /* incoming connection */
                    incomming();
                } else {
                    /* incoming data */
                    read_data(fd);
                }
            }
    }
    dbg("Exiting main loop.");
}

int main() {
    if (!init()) {
        cleanup();
        return EXIT_FAILURE;
    }
    loop();
    cleanup();
    return EXIT_SUCCESS;
}
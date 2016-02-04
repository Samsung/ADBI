#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "signal.h"
#include "protocol.h"

static int comm_server_sock = -1;
static int comm_connection_sock = -1;

/* Set up the file descriptor for asynchronous operation.  Set the O_NONBLOCK and O_ASYNC flags. Also make sure the
 * current process will receive the SIGIO signals related to this file descriptor. */
static bool comm_enable_async_io(int fd) {
    /* Set flags. */
    int flags = fcntl(fd, F_GETFL, 0);
    
    if (flags == -1)
        goto fail;
        
    if (fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1)
        goto fail;
        
    /* Make sure we'll receive the SIGIO signals. */
    if (fcntl(fd, F_SETOWN, getpid()) == -1)
        goto fail;
        
    debug("Asynchronous I/O operation enabled on fd %i.", fd);
    return true;
fail:
    error("Error enabling asynchronous I/O operation on fd %i: %s.", fd, strerror(errno));
    return false;
}

/* Set a single option on the server socket using setsockopt. */
static bool comm_setsockopt(int option, int value) {
    int optval = value;
    socklen_t optlen = sizeof(optval);
    return (setsockopt(comm_server_sock, SOL_SOCKET, option, &optval, optlen) >= 0);
}

/* Close a socket gracefully by first shutting it down and then closing the fd. */
static void close_socket(int sock) {
    /* Shutdown the connection */
    if (shutdown(sock, SHUT_RDWR) < 0) {
        /* Shutdown failed. This is okay only if the socket is not connected anymore (this occurs on
         * unexpected disconnection).  All other errors here are unacceptable and mean that we have a bug. */
        adbi_assure(errno == ENOTCONN);
    }
    
    /* Close the connection. We need to do this in a loop, because close can fail. See man for details. */
    while (close(sock)) {
        adbi_assure(errno != EBADF);
    }
}

/* Close client connection if not closed already. */
static void comm_close_connection() {
    if (comm_connection_sock >= 0) {
        close_socket(comm_connection_sock);
        comm_connection_sock = -1;
        info("Client connection closed.");
    }
}

/* Close server socket if not closed already. */
static void comm_close_server() {
    if (comm_server_sock >= 0) {
        close_socket(comm_server_sock);
        comm_server_sock = -1;
        info("Server socket closed.");
    }
}

/* Open server socket on given port and start listening.  Return success flag.  This function doesn't call accept, so
 * it doesn't block. */
static int comm_init_server(int port) {

    struct sockaddr_in address;
    
    assert(comm_server_sock == -1);
    
    comm_server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (comm_server_sock < 0) {
        error("Error creating TCP socket.");
        goto fail;
    }
    
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    
    if (!comm_setsockopt(SO_REUSEADDR, 1))
        warning("Can not set SO_REUSEADDR option of listening socket. You may have trouble creating sockets at this "
                "address after adbiserver exits.");
                
    if (bind(comm_server_sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
        error("Error binding TCP socket to port %i.", port);
        goto fail;
    }
    
    comm_enable_async_io(comm_server_sock);
    
    if (listen(comm_server_sock, 1) < 0) {
        error("Error listening on TCP socket at port %i.", port);
        goto fail;
    }
    
    info("Listening on port %d.", port);
    
    return 1;
    
fail:
    if (comm_server_sock >= 0)
        close(comm_server_sock);
    comm_server_sock = -1;
    return 0;
}

/* Try to accept a client connection on the server socket.  Do not block.  If there's no client waiting for an accept,
 * return false immediately.  If a client connects, close server socket, so no other client can connect, and return
 * true. */
static bool comm_answer() {
    info("Waiting for connection.");
    
    comm_connection_sock = accept(comm_server_sock, NULL, NULL);
    
    if (comm_connection_sock < 0) {
        if (errno == EWOULDBLOCK) {
            /* Nobody's there. */
        } else
            error("Connection failed: %s", strerror(errno));
        return false;
    }
    
    /* We don't expect more than one client, so we close the listening socket. */
    comm_close_server();
    
    /* Setup asynchronous IO */
    comm_enable_async_io(comm_connection_sock);
    
    info("Accepted client connection.");
    return true;
}

/* Send out count bytes of data from buf. The function blocks until all data is sent or an error occurs.
 * Return success flag. */
static bool comm_send(const void * buf, unsigned int count) {

    while (count > 0) {
        ssize_t sent_bytes = write(comm_connection_sock, buf, count);
        
        if (sent_bytes >= 0) {
            count -= sent_bytes;
            buf += sent_bytes;
        } else {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* That's ok, retry. */
            } else {
                /* Error */
                error("Error sending data: %s.", strerror(errno));
                return false;
            }
        }
    }
    return true;
}


/* Read count bytes and store them in buf. If wait is true, the function blocks until count bytes are available.
 * Returns:
 *   0  on failure (e.g. disconnect)
 *   1  on success
 *  -1  if wait is zero and no data is available  */
static int comm_read_raw(void * buf, unsigned int count, bool wait) {

    while (count > 0) {
        ssize_t received_bytes = read(comm_connection_sock, buf, count);
        
        if (received_bytes > 0) {
            count -= (int) received_bytes;
            buf += (int) received_bytes;
            /* We received at least one byte, so now we always wait for count bytes to become available. */
            wait = true;
        } else {
            if (received_bytes == 0) {
                warning("Connection shut down.");
                return 0;
            } else if (errno != EWOULDBLOCK) {
                error("Error receiving data: %s.", strerror(errno));
                return 0;
            } else if (!wait) {
                /* operation would block. */
                return -1;
            }
        }
    }
    
    return 1;
}

static void comm_got_data() {
    packet_header_t head;
    
    while (1) {
    
        switch (comm_read_raw(&head, sizeof(packet_header_t), false)) {
            case 1:
                /* Success, read payload. */
                {
                    packet_t * request = packet_create(&head);
                    const packet_t * response;
                    if (comm_read_raw(request->payload, head.length, true)) {
                        response = handle_packet(request);
                        if (response) {
                            debug("Sending response packet (%u bytes).", (unsigned int) response->head.length);
                            comm_send(&response->head, sizeof(packet_header_t));
                            comm_send(response->payload, response->head.length);
                        }
                    } else {
                        /* Error, disconnect. */
                        comm_close_connection();
                        comm_init_server(9999);
                        return;
                    }
                    packet_free(request);
                }
                return;
            case 0:
                /* Error, disconnect. */
                comm_close_connection();
                comm_init_server(9999);
                return;
            case -1:
                /* Timeout, no more data. */
                return;
        }
    }
}

/* This function is called when we receive a SIGIO. */
void comm_handle_io() {
    if (comm_connection_sock >= 0) {
        /* Nobody is connected, maybe there's an incoming connection. */
        comm_got_data();
    } else if (comm_server_sock >= 0) {
        /* We have a connection */
        comm_answer();
    }
}

/* Communication clean-up function. */
void comm_cleanup() {
    comm_close_connection();
    comm_close_server();
}

/* Communication initialization function. */
bool comm_init() {
    if (comm_init_server(9999))
        return 1;
    else {
        fatal("Error initializing communication.");
        return 0;
    }
}

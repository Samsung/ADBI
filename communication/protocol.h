#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef packed_struct packet_header_t {
    uint32_t type;      /* packet type */
    uint32_t seq;       /* packet sequence number */
    uint32_t length;    /* packet size */
} packet_header_t;

typedef struct packet_t {
    packet_header_t head;
    void * payload;
} packet_t;

bool protocol_init();
void protocol_cleanup();

packet_t * packet_create(const packet_header_t * head);
void packet_free(packet_t * packet);

const packet_t * handle_packet(const packet_t * request);

#endif

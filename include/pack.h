#ifndef _PACK_H_
#define _PACK_H_

#include <stdint.h>

/* for client side */
struct pack
{
  uint32_t id;
  uint32_t seq;
  uint16_t type;
  uint16_t tag;
  uint32_t datalen;

  /* reserved, opaque across network */
  //struct list_head q;

  char data[1];
};

/* for server side */
typedef struct pack pack_data;

/* packet types */
enum {
  PACKET_HBEAT,
  PACKET_AUDIO,
  PACKET_ENOUNCE,
};

#define P_HTON(p) \
do { \
  (p)->id   = htonl((p)->id); \
  (p)->seq  = htonl((p)->seq); \
  (p)->type = htons((p)->type); \
  (p)->tag  = htons((p)->tag); \
  (p)->datalen = htonl((p)->datalen); \
}while(0)

#define P_NTOH(p) \
do { \
  (p)->id   = ntohl((p)->id); \
  (p)->seq  = ntohl((p)->seq); \
  (p)->type = ntohs((p)->type); \
  (p)->tag  = ntohs((p)->tag); \
  (p)->datalen = ntohl((p)->datalen); \
}while (0)


#endif

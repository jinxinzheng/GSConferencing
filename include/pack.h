#ifndef _PACK_H_
#define _PACK_H_

#include <stdint.h>

/* for client side */
struct pack
{
  uint32_t id;
  uint32_t seq;
  uint8_t type;
  uint8_t tag;
  uint16_t datalen;

  char data[1];
}
__attribute__((packed));

/* for server side */
typedef struct pack pack_data;

/* packet types */
enum {
  PACKET_HBEAT,
  PACKET_AUDIO,
  PACKET_ENOUNCE,
  PACKET_REPEAT_REQ,
  PACKET_SEQ_OUTDATE,
  PACKET_AUDIO_ZERO,  /* silence compressed */
  PACKET_VIDEO,
  PACKET_UCMD,
  PACKET_MIC_OP, /* for direct audio mix notify mic open/close */
};

/* cmd sent in udp packet.
 * usually broadcasted cmd. */
struct pack_ucmd
{
  uint16_t cmd;
  union {
    uint8_t data[6];
    struct {
      uint8_t tag;
      uint8_t rep;
    } interp;
  } u;
  uint8_t type; /* type must be compatible with type of struct pack,
                   and always set to PACKET_UCMD. */
}
__attribute__((packed));

enum {
  UCMD_INTERP_REP_TAG,   /* interpreter has set or reset replicate tag */
};

#define P_HTON(p) \
do { \
  (p)->id   = htonl((p)->id); \
  (p)->seq  = htonl((p)->seq); \
  (p)->datalen = htons((p)->datalen); \
}while(0)

#define P_NTOH(p) \
do { \
  (p)->id   = ntohl((p)->id); \
  (p)->seq  = ntohl((p)->seq); \
  (p)->datalen = ntohs((p)->datalen); \
}while (0)

#define PACK_LEN(p)  (offsetof(struct pack, data) + (p)->datalen)


#endif

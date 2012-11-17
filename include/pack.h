#ifndef _PACK_H_
#define _PACK_H_

#include <stdint.h>

/* for client side */
struct pack
{
  uint32_t id; /* the highest byte of id may carry
                  some extra info, e.g. audio type. */
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
    struct {
      uint16_t seq;
      uint8_t mode;
      uint8_t tag;
    } brcast_cmd;
  } u;
  uint8_t type; /* type must be compatible with type of struct pack,
                   and always set to PACKET_UCMD. */
  uint8_t unuse;
  uint16_t datalen;
  uint8_t data[1];
}
__attribute__((packed));

enum {
  /* interpreter has set or reset replicate tag */
  UCMD_INTERP_REP_TAG,

  /* normal cmd that is suitable for broadcast.
     the original cmd is stored in data[],
     including the ending \n and \0. */
  UCMD_BRCAST_CMD,
};

enum {
  BRCMD_MODE_ALL,

  /* tag id is pack_ucmd.u.brcast_cmd.tag */
  BRCMD_MODE_TAG,

  /* multi cast. the dest IDs are stored in the
   * leading space in pack_ucmd.data:
   * [0..4] : number of IDs.
   * [5...]: array of IDs, 4 bytes each.
   * following the ID array is the payload cmd. */
  BRCMD_MODE_MULTI,
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

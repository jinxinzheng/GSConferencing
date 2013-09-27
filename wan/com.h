#ifndef __COM_H__
#define __COM_H__


#include  <stdint.h>
#include  <stddef.h>

struct com_pack {
  uint8_t ver;
  uint8_t cmd;
  uint16_t len;
  union {
    struct {
      uint8_t tag;
      uint8_t type;
      uint16_t seq;
      uint8_t data[1];
    } audio;
  }u;
};

#define HEAD_SIZE (offsetof(struct com_pack, u))

enum {
  CMD_CONNECT,
  CMD_AUDIO,
};

enum {
  AUDIO_RAW,
  AUDIO_AAC,
  AUDIO_AMR,
};


#endif  /*__COM_H__*/

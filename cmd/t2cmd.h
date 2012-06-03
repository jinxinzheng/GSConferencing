#ifndef  __T2CMD_H__
#define  __T2CMD_H__


struct type2_cmd
{
  uint8_t type;
  uint8_t cmd;
  uint16_t len;
  char data[1];
}
__attribute__((packed));

#define T2CMD_SIZE(p) \
  offsetof(struct type2_cmd, data) + (p)->len

enum {
  T2CMD_NONE,
  T2CMD_SUBS_LIST,
};

struct t2cmd_subs_list
{
  uint16_t tag;
  uint16_t count;
  struct {
    uint32_t id;
    uint32_t addr;
    uint16_t port;
    uint16_t flag;
  } subs[1];
};


#endif  /*__T2CMD_H__*/

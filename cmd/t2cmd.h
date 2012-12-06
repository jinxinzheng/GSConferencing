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
  T2CMD_INTERP_REP,

  T2CMD_OTHER = 64,
  T2CMD_SUBS_LIST,
  T2CMD_SUB,
};

struct t2cmd_interp_rep
{
  uint8_t tag;
  uint8_t rep;
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

struct t2cmd_sub
{
  uint32_t id;
  uint32_t addr;
  uint16_t port;
  uint16_t flag;
  uint8_t sub;
  uint8_t tag;
};


#endif  /*__T2CMD_H__*/

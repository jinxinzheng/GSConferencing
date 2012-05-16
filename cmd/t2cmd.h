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

enum {
  T2CMD_NONE,
  T2CMD_SUBS_LIST,
};

struct t2cmd_subs_list
{
  uint16_t tag;
  uint16_t count;
};


#endif  /*__T2CMD_H__*/

#include "cmd_handler.h"

#define DEVLIST_TO_IDLIST(str, head, listmember) \
do{ \
  int _l = 0; \
  struct list_head *_t; \
  struct device *_d; \
\
  list_for_each(_t, head) \
  { \
    _d = list_entry(_t, struct device, listmember); \
    LIST_ADD_NUM(str, _l, (int)_d->id); \
  } \
  LIST_END(str, _l); \
}while(0)

int handle_cmd_msgctrl(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  char buf[1024];
  struct device *d;
  char *p;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("query")
  {
    struct group *g;

    THIS_DEVICE(cmd, d);

    g = d->group;

    DEVLIST_TO_IDLIST(buf, &g->device_head, list);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, buf);
    REP_END(cmd);
  }

  SUBCMD("send")
  {
    NEXT_ARG(p);

    REP_OK(cmd);

    if (strcmp(p, "all") == 0)
    {
      SEND_TO_GROUP_ALL(cmd);
    }
    else
    {
      SEND_TO_IDLIST(cmd, p);
    }
  }
}

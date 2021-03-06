#include "cmd_handler.h"
#include "brcmd.h"

#define DEVLIST_TO_IDLIST(str, head, listmember) \
  list_TO_NUMLIST(str, head, struct device, listmember, id)

static int cmd_msgctrl(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  char buf[10240];
  struct device *d;
  char *p;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("query")
  {
    struct group *g;

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
      brcast_cmd_to_all(cmd);
    }
    else
    {
      SEND_TO_IDLIST(cmd, p);
    }
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(msgctrl);

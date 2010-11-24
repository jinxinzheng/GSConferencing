#include "cmd_handler.h"
#include <string.h>
#include "dev.h"
#include "sys.h"

#define SEND_TO_GROUP_ALL(cmd) \
do { \
  struct group *_g; \
  struct device *_d; \
  struct list_head *_t; \
  if (!(_d = get_device((cmd)->device_id))) \
    return 1; \
  _g = _d->group; \
  list_for_each(_t, &_g->device_head) \
  { \
    _d = list_entry(_t, struct device, list); \
    sendto_dev_tcp((cmd)->rep, (cmd)->rl, _d); \
  } \
} while(0)

int handle_cmd_discctrl(struct cmd *cmd)
{
  char *subcmd, *p;
  char buf[1024];
  int a=0;
  int i,l;

  struct device *d;

#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) \
    return 1;

  NEXT_ARG(subcmd);

  if (0) ;

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)

  SUBCMD("query")
  {
    REP_ADD(cmd, "OK");
    REP_ADD(cmd, "conv1,conv2,conv3");
    REP_END(cmd);
  }

  SUBCMD("select")
  {
    NEXT_ARG(p);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, "101,102,103");
    REP_END(cmd);

    for (i=101; i<=103; i++)
    {
      if (d = get_device(i))
        sendto_dev_tcp(cmd->rep, cmd->rl, d);
    }
  }

  SUBCMD("request")
  {
    REP_OK(cmd);
  }

  SUBCMD("status")
  {
    NEXT_ARG(p);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, "102,103");
    REP_END(cmd);
  }

  SUBCMD("close")
  {
    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("stop")
  {
    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("forbid")
  {
    NEXT_ARG(p);
    i = atoi(p);

    REP_OK(cmd);

    if (d = get_device(i))
      sendto_dev_tcp(cmd->rep, cmd->rl, d);
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

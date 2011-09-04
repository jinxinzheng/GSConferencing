#include "cmd_handler.h"

static int cmd_servicecall(struct cmd *cmd)
{
  int a=0;

  struct device *d;
  char *p;
  int request;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  NEXT_ARG(p);

  request = atoi(p);

  /* send to the special 'manager' virtual device */
  send_cmd_to_dev_id(cmd, 1);

  REP_OK(cmd);

  return 0;
}

CMD_HANDLER_SETUP(servicecall);

#include "cmd_handler.h"
#include "db/db.h"

int handle_cmd_sysconfig(struct cmd *cmd)
{
  int a=0;

  char *cid;
  char *tcmd;
  int i;

  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(cid);
  NEXT_ARG(tcmd);

  if( (i = atoi(cid)) )
  {
    send_cmd_to_dev_id(cmd, i);
  }
  else
  {
    send_to_group_all(cmd, d->group);
  }

  REP_OK(cmd);

  return 0;
}

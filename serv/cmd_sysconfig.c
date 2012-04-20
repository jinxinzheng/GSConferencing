#include "cmd_handler.h"
#include "db/db.h"
#include "brcmd.h"

static int cmd_sysconfig(struct cmd *cmd)
{
  int a=0;

  char *cid;
  char *tcmd;
  int i;

  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(cid);
  NEXT_ARG(tcmd);

  REP_OK(cmd);

  if( (i = atoi(cid)) )
  {
    send_cmd_to_dev_id(cmd, i);
  }
  else
  {
    brcast_cmd_to_all(cmd);
  }

  return 0;
}

CMD_HANDLER_SETUP(sysconfig);

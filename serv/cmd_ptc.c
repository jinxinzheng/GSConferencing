/* direct ptc control */
#include  "cmd_handler.h"

static int cmd_ptc(struct cmd *cmd)
{
  int a=0;

  char *ptcid;
  char *ptcmd;
  int id;

  struct device *d;
  THIS_DEVICE(cmd, d);

  NEXT_ARG(ptcid);
  NEXT_ARG(ptcmd);

  REP_OK(cmd);

  if( (id = atoi(ptcid)) )
  {
    send_cmd_to_dev_id(cmd, id);
  }

  return 0;
}

CMD_HANDLER_SETUP(ptc);

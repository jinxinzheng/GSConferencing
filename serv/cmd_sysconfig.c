#include "cmd_handler.h"
#include "db/db.h"

int handle_cmd_sysconfig(struct cmd *cmd)
{
  int a=0;

  char *key;
  char *val;

  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(key);
  NEXT_ARG(val);

  /* save key/val to sysconfig */
  db_sysconfig_set(key, val);

  REP_OK(cmd);

  send_to_group_all(cmd, d->group);

  return 0;
}

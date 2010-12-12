#include "cmd_handler.h"

int handle_cmd_servicecall(struct cmd *cmd)
{
  int a=0;

  struct device *d;
  char *p;
  int request;

  NEXT_ARG(p);

  request = atoi(p);

  /* todo: send the request to the manager */

  REP_OK(cmd);

  return 0;
}

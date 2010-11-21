#include "cmd_handler.h"
#include <string.h>

int handle_cmd_convctrl(struct cmd *cmd)
{
  char *subcmd, *p;
  char buf[1024];
  int a=0;
  int l;

#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) \
    return 1;

  NEXT_ARG(subcmd);

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)

  if (0) ;

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
  }

  SUBCMD("stop")
  {
    REP_OK(cmd);
  }

  SUBCMD("forbid")
  {
    NEXT_ARG(p);

    REP_OK(cmd);
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

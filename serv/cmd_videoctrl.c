#include "cmd_handler.h"

static struct
{
  const char *name;
}
va = {"va"},
vb = {"vb"},
vc = {"vc"},
*videos[] = {
  &va, &vb, &vc
};

int handle_cmd_videoctrl(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  int i;
  char *p;

  struct device *d;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("query")
  {
    char buf[4096];
    MAKE_STRLIST(buf, videos, sizeof videos/sizeof videos[0], name);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, buf);
    REP_END(cmd);
  }

  SUBCMD("select")
  {
    NEXT_ARG(p);
    i = atoi(p);

    REP_OK(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

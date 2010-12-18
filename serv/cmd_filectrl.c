#include "cmd_handler.h"

static struct
{
  const char *name;
}
fa = {"file1.txt"},
fb = {"file2.txt"},
fc = {"file3.txt"},
*files[] = {
  &fa, &fb, &fc
};

int handle_cmd_filectrl(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  int i;
  char *p;
  
  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("query")
  {
    char buf[4096];
    MAKE_STRLIST(buf, files, sizeof files/sizeof files[0], name);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, buf);
    REP_END(cmd);
  }

  SUBCMD("select")
  {
    NEXT_ARG(p);
    i = atoi(p);

    REP_OK(cmd);

    send_file_to_dev(files[i]->name, d);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

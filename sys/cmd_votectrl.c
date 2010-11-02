#include "cmd_handler.h"
#include <string.h>
#include <stdio.h>

int handle_cmd_votectrl(struct cmd *cmd)
{
  char *scmd, *p;
  char buf[1024];
  int ai=0, i,l;

  /* todo: replace with database */
  static char *db[][2] = {
    {"vote1", "1,2,3,4"},
    {"vote2", "1,3,5,6"},
    {"vote3", "all"},
    {NULL}
  };
  static int dbl = 3;

  scmd = cmd->args[ai++];
  if (!scmd)
    return 1;

  if (strcmp(scmd, "query") == 0)
  {
    REP_ADD(cmd, "OK");

    l=0;
    for (i=0; i<dbl; i++)
    {
      LIST_ADD(buf, l, db[i][0]);
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);
  }
  else if (strcmp(scmd, "select") == 0)
  {
    p = cmd->args[ai++];
    if (!p)
      return 1;

    i = atoi(p);
    if (i >= dbl) {
      fprintf(stderr, "invalid vote select number.\n");
      return 1;
    }

    REP_ADD(cmd, "OK");

    p = db[i][1];
    REP_ADD(cmd, p);

    REP_END(cmd);
  }
  else
    return 2;

  return 0;
}

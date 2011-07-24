#include "cmd_handler.h"
#include "db/md.h"

static struct db_video *table[1024];
static int tblen;

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

    tblen = md_get_array_video(table);

    MAKE_STRLIST(buf, table, tblen, name);

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, buf);
    REP_END(cmd);
  }

  SUBCMD("select")
  {
    NEXT_ARG(p);
    i = atoi(p);

    if( i >= tblen )
    {
      return ERR_OTHER;
    }

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, table[i]->path);
    REP_END(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(videoctrl);

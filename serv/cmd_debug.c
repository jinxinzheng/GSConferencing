#include "cmd_handler.h"
#include "tag.h"
#include <arpa/inet.h>

int handle_cmd_debug(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  struct device *d;
  int i,l;
  char *p;
  char buf[2048];

  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("print")
  {
    char *obj;

    REP_ADD(cmd, "OK");

    NEXT_ARG(p);
    obj = p;
    if (strcmp("tag", obj) == 0)
    {
      struct tag *t;
      NEXT_ARG(p);
      i = atoi(p);
      if (!(t = get_tag(TAGUID(d->group->id, i))))
      {
        REP_ADD(cmd, "not found");
      }
      else
      {
        l=0;
        for (i=0; i<t->bcast_size; i++)
        {
          p = inet_ntoa(t->bcasts[i]->sin_addr);
          LIST_ADD(buf, l, p);
        }
        LIST_END(buf, l);

        REP_ADD(cmd, buf);
      }
    }

    REP_END(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

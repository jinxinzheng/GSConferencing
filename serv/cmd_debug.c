#include "cmd_handler.h"
#include "tag.h"
#include <arpa/inet.h>
#include "init.h"

static int cmd_debug(struct cmd *cmd)
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
        REP_PRF(cmd, "\n%d not found", i);
      }
      else
      {
        l=0;
        for (i=0; i<t->bcast_size; i++)
        {
          p = inet_ntoa(t->bcasts[i]->sin_addr);
          LIST_ADD(buf, l, p);
        }

        REP_ADD(cmd, buf);
      }
    }
    else if( strcmp("dev", obj) == 0 )
    {
      struct group *g = d->group;

      list_for_each_entry(d, &g->device_head, list)
      {
        REP_PRF(cmd, "\n%d", (int)d->id);
        REP_PRF(cmd, "\n addr=%s:%d", inet_ntoa(d->addr.sin_addr), ntohs(d->addr.sin_port));
        REP_PRF(cmd, "\n tag=%d", (int)d->tag->id);
        REP_PRF(cmd, "\n sub [0]=%d, [1]=%d",
          d->subscription[0]? (int)d->subscription[0]->id : 0,
          d->subscription[1]? (int)d->subscription[1]->id : 0 );
      }
    }
    else
    {
      REP_PRF(cmd, "\n'%s' unknown", obj);
    }

    REP_END(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(debug);

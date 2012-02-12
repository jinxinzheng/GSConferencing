#include "cmd_handler.h"
#include "include/types.h"
#include "interp.h"

static int cmd_interp(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  struct device *d;
  struct tag *t;
  char *p;

  THIS_DEVICE(cmd, d);
  t = d->tag;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("set_mode")
  {
    int mode;

    NEXT_ARG(p);
    mode = atoi(p);

    switch ( mode )
    {
      case INTERP_NO :
      {
        interp_del_rep(t);
      }
      break;

      case INTERP_OR :
      {
        int gid = d->group->id;
        int tuid = TAGUID(gid, 1);
        struct tag *rep = get_tag(tuid);

        interp_set_rep_tag(t, rep);
      }
      break;

      case INTERP_RE :
      {
        /* dup the current subscription. */
        struct tag *rep;
        rep = d->subscription[0];
        if( !rep )
          rep = d->subscription[1];
        if( rep )
        {
          interp_set_rep_tag(t, rep);
        }
      }
      break;

      default :
      break;
    }

    t->interp.mode = mode;

    REP_OK(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(interp);

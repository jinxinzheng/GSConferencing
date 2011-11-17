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
      if( t->interp.dup )
      {
        interp_del_dup_tag(t);
      }
      break;

      case INTERP_OR :
      {
        int gid = d->group->id;
        long tuid = TAGUID(gid, 1);
        struct tag *dup = get_tag(tuid);

        interp_add_dup_tag(t, dup);
      }
      break;

      case INTERP_RE :
      {
        /* dup the current dev's subscription. */
        struct tag *dup = t->interp.curr_dev->subscription[0];
        if( !dup )
          dup = t->interp.curr_dev->subscription[1];
        if( dup )
        {
          interp_add_dup_tag(t, dup);
        }
      }
      break;

      default :
      break;
    }

    t->interp.mode = mode;

    REP_OK(cmd);

    send_to_tag_all(cmd, t);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(interp);

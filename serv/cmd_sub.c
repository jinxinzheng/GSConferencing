#include "cmd_handler.h"
#include <string.h>
#include "sys.h"
#include "devctl.h"
#include "cast.h"

static int cmd_sub(struct cmd *cmd)
{
  char *p;

  p = cmd->args[0];

  if (!p)
    return 1;
  else do
  {
    struct device *d;
    struct tag *t;
    long tid, gid;
    long long tuid;
    int unsub = 0;

    THIS_DEVICE(cmd, d);

    gid = d->group->id;
    tid = atoi(p);

    if( tid == 0 )
    {
      return ERR_OTHER;
    }

    if( tid < 0 )
    {
      tid = -tid;
      unsub = 1;
    }

    tuid = TAGUID(gid, tid);
    t = get_tag(tuid);

    if (d) {
      if( unsub )
      {
        if( t )
          dev_unsubscribe(d, t);
        break;
      }

      if (!t)
        /* create an 'empty' tag that has no registered device.
         * this ensures the subscription not lost if there are
         * still not any device of the tag registered. */
        t = tag_create(gid, tid);

      dev_subscribe(d, t);
    }
  } while(0);

  REP_OK(cmd);
  return 0;
}

CMD_HANDLER_SETUP(sub);

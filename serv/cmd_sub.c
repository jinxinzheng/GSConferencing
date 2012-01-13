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

    if( unsub )
    {
      t = get_tag(TAGUID(gid, tid));
      if( t )
        dev_unsubscribe(d, t);
      break;
    }
    else
    {
      t = request_tag(gid, tid);
      dev_subscribe(d, t);
    }
  } while(0);

  REP_OK(cmd);
  return 0;
}

CMD_HANDLER_SETUP(sub);

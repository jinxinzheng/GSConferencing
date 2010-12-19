#include "cmd_handler.h"
#include <string.h>
#include "sys.h"
#include "devctl.h"
#include "cast.h"

int handle_cmd_sub(struct cmd *cmd)
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

    THIS_DEVICE(cmd, d);

    gid = d->group->id;
    tid = atoi(p);
    tuid = TAGUID(gid, tid);
    t = get_tag(tuid);

    if (d) {
      if (0 == tid)
      {
        dev_unsubscribe(d);
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

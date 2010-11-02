#include "cmd_handler.h"
#include <string.h>
#include "sys.h"
#include "devctl.h"

int handle_cmd_sub(struct cmd *cmd)
{
  char *p;

  p = cmd->args[0];

  if (!p)
    return 1;
  else
  {
    struct device *d;
    struct tag *t;
    long tid, gid;
    long long tuid;

    d = get_device(cmd->device_id);

    gid = d->group->id;
    tid = atoi(p);
    tuid = TAGUID(gid, tid);
    t = get_tag(tuid);

    if (d) {
      if (!t)
        /* create an 'empty' tag that has no registered device.
         * this ensures the subscription not lost if there are
         * still not any device of the tag registered. */
        t = tag_create(gid, tid);

      dev_subscribe(d, t);
    }
  }
  REP_OK(cmd);
  return 0;
}

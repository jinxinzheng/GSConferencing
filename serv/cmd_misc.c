#include "cmd_handler.h"
#include <time.h>
#include <include/types.h>
#include <include/debug.h>

int handle_cmd_synctime(struct cmd *cmd)
{
  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);

  REP_ADD(cmd, "OK");
  REP_ADD_NUM(cmd, (int)t.tv_sec);
  REP_ADD_NUM(cmd, (int)t.tv_nsec);
  REP_END(cmd);

  return 0;
}

int handle_cmd_report_cyc_ctl(struct cmd *cmd)
{
  struct group *g;
  struct device *d;

  THIS_DEVICE(cmd, d);

  g = d->group;

  if( g->cyctl )
  {
    trace_warn("received multiple cyc-ctl dev report: was %ld, to %ld\n",
      g->cyctl->id, d->id);
  }

  g->cyctl = d;

  return 0;
}

int handle_cmd_sys_stats(struct cmd *cmd)
{
  struct group *g;
  struct device *d;

  THIS_DEVICE(cmd, d);

  g = d->group;

  REP_ADD(cmd, "OK");

  REP_ADD_NUM(cmd, g->stats.dev_count[DEVTYPE_NORMAL]);
  REP_ADD_NUM(cmd, g->stats.dev_count[DEVTYPE_CHAIR]);
  REP_ADD_NUM(cmd, g->stats.dev_count[DEVTYPE_INTERP]);

  REP_END(cmd);

  return 0;
}

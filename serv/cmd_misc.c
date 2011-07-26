#include "cmd_handler.h"
#include <time.h>
#include <include/types.h>
#include <include/debug.h>
#include "db/db.h"

static int cmd_synctime(struct cmd *cmd)
{
  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);

  REP_ADD(cmd, "OK");
  REP_ADD_NUM(cmd, (int)t.tv_sec);
  REP_ADD_NUM(cmd, (int)t.tv_nsec);
  REP_END(cmd);

  return 0;
}

static int cmd_report_cyc_ctl(struct cmd *cmd)
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

static int cmd_set_ptc(struct cmd *cmd)
{
  int a=0;

  struct device *d;
  char *p;

  int cid;
  int ptcid;
  char *ptcmd;
  struct device *c;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(p);
  cid = atoi(p);

  NEXT_ARG(p);
  ptcid = atoi(p);

  NEXT_ARG(p);
  ptcmd = p;

  if( (c = get_device(cid)) )
  {
    c->db_data->ptc_id = ptcid;
    strcpy(c->db_data->ptc_cmd, ptcmd);

    device_save(c);
  }

  REP_OK(cmd);

  return 0;
}

static int cmd_sys_stats(struct cmd *cmd)
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

CMD_HANDLER_SETUP(synctime);
CMD_HANDLER_SETUP(report_cyc_ctl);
CMD_HANDLER_SETUP(set_ptc);
CMD_HANDLER_SETUP(sys_stats);

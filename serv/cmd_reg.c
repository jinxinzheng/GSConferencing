#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "devctl.h"
#include "include/cksum.h"
#include "include/debug.h"
#include "../config.h"
#include "db/md.h"

static void get_client_info(char *info, const struct device *d)
{
  int l = 0;
  struct group *g = d->group;
  struct db_device *dd = d->db_data;
  struct db_group *dg = g->db_data;

  struct db_discuss *dsc = g->discuss.current;

  struct db_vote *dvt = d->vote.v ? g->vote.current : NULL;

#define ADD_ARG(fmt, a...) SEP_ADD(info, l, ":", fmt, ##a)

  ADD_ARG("user_name=%s", dd->user_name);
  ADD_ARG("user_gender=%d", dd->user_gender);
  ADD_ARG("tag=%d", dd->tagid);
  ADD_ARG("sub=%d,%d", dd->sub1, dd->sub2);

  ADD_ARG("discuss_mode=%d", dg->discuss_mode);
  ADD_ARG("discuss_num=%d", dsc? g->discuss.curr_num : -1);
  ADD_ARG("discuss_chair=%d", g->chairman == d);
  ADD_ARG("discuss_open=%d", dd->discuss_open);

  ADD_ARG("regist_start=%d", dg->regist_start);
  ADD_ARG("regist_mode=%d", dg->regist_mode);
  ADD_ARG("regist_reg=%d", d->regist.reg);

  ADD_ARG("vote_num=%d", dvt? g->vote.curr_num : -1);
  ADD_ARG("vote_master=%d", dvt? dd->vote_master:0);
  ADD_ARG("vote_type=%d", dvt? dvt->type:0);
  ADD_ARG("vote_total=%d", d->vote.v? d->vote.v->n_members:0);
  ADD_ARG("vote_results=%s", dg->vote_results);
  ADD_ARG("vote_choice=%d", dd->vote_choice);
}

int handle_cmd_reg(struct cmd *cmd)
{
  int a=0;

  char *p;
  struct device *d;
  char sum[64];
  unsigned long n;
  int type;
  char *pass;
  int port;
  char *bcast;

  int did = cmd->device_id;

  char buf[1024];

  NEXT_ARG(p);
  type = atoi(p);

  NEXT_ARG(p);
  pass = p;

  NEXT_ARG(p);
  port = atoi(p);

  NEXT_ARG(p);
  bcast = p;

  /* authenticate the passwd based on id and type */
  n = (unsigned long)cmd->device_id ^ (unsigned long)type;
  cksum(&n, sizeof n, sum);
  if (strcmp(pass, sum) != 0)
  {
    trace_warn("authenticate fail\n");
    return 1;
  }

  if (port <= 0)
    return 1;

  d = get_device(did);

  if( !d )
  {
    d = dev_create(did);
  }

  d->addr = *cmd->saddr;
  d->addr.sin_port = htons(port);

  d->fileaddr = d->addr;
  d->fileaddr.sin_port = htons(port+1);

  if (strcmp("none", bcast) == 0)
    d->bcast.sin_addr.s_addr = 0;
  else
  {
    d->bcast.sin_addr.s_addr = inet_addr(bcast);
    d->bcast.sin_port = htons(BRCAST_PORT);
  }

  dev_update_data(d);

  if (dev_register(d) != 0)
  {
    /* existing dev ok */
  }

  get_client_info(buf, d);

  REP_ADD(cmd, "OK");
  REP_ADD(cmd, buf);
  REP_END(cmd);

  return 0;
}

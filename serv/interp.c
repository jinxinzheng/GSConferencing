#include "include/pack.h"
#include "tag.h"
#include "network.h"
#include <unistd.h>
#include <include/util.h>
#include <cmd/t2cmd.h>
#include "brcmd.h"

/* notify all clients that the tag
 * is replicated. */
static void notify_interp_rep(struct tag *t)
{
  char buf[100];
  struct type2_cmd *c = (struct type2_cmd *) buf;
  struct t2cmd_interp_rep *data = (struct t2cmd_interp_rep *) c->data;
  c->type = 2;
  c->cmd = T2CMD_INTERP_REP;
  c->len = sizeof(struct t2cmd_interp_rep);
  data->tag = t->id;
  data->rep = t->interp.rep? t->interp.rep->id : 0;
  brcast_data_to_all(c, T2CMD_SIZE(c));
}

void interp_set_rep_tag(struct tag *t, struct tag *rep)
{
  t->interp.rep = rep;
  notify_interp_rep(t);
}

void interp_del_rep(struct tag *t)
{
  t->interp.rep = NULL;
  notify_interp_rep(t);
}

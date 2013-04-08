#include "cmd_handler.h"
#include "db/md.h"
#include "devctl.h"
#include  <include/types.h>
#include  <include/util.h>
#include  <cmd/t2cmd.h>

static int cmd_get_tags(struct cmd *cmd)
{
  iter it = NULL;
  struct db_tag *dt;
  char buf[1024];
  int l=0;

  while( (dt = md_iterate_tag_next(&it)) )
  {
    LIST_ADD_FMT(buf, l, "%d:%s", dt->id, dt->name);
  }

  REP_ADD(cmd, "OK");
  REP_ADD_STR(cmd, buf, l);
  REP_END(cmd);

  return 0;
}

static int cmd_switch_tag(struct cmd *cmd)
{
  int a=0;

  char *p;
  int gid;
  int tid;

  struct device *d;
  struct tag *t;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(p);

  tid = atoi(p);
  if( tid <= 0 )
  {
    return ERR_OTHER;
  }

  REP_OK(cmd);

  if( tid == d->tag->id )
  {
    return 0;
  }

  gid = d->group->id;

  t = request_tag(gid, tid);

  tag_del_device(d);

  tag_add_device(t, d);

  d->tag = t;

  tag_check_ucast(t, d);

  if( d->type == DEVTYPE_INTERP &&
     (d->tag->interp.mode == INTERP_OR ||
      d->tag->interp.mode == INTERP_RE) )
  {
    apply_interp_mode(d->tag, d);
  }

  /* update db */
  {
    struct db_device *dbd;
    if( (dbd = d->db_data) )
    {
      dbd->tagid = tid;
      device_save(d);
    }
  }

  return 0;
}

static int cmd_get_subs(struct cmd *cmd)
{
  int a=0;

  struct device *d, *e;
  char *p;
  int tid;
  struct list_head *h;

  struct type2_cmd *rep;
  struct t2cmd_subs_list *hdr;
  int count;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(p);
  tid = atoi(p);
  if( tid <= 0 )
    return ERR_OTHER;

  if( d->type != DEVTYPE_UCAST_AUDIO )
    return ERR_OTHER;

  if( d->tag->id != tid )
    return ERR_OTHER;

  /* reply with 'type 2' cmd */
  rep = (struct type2_cmd *) cmd->rep;
  rep->type = 2;
  rep->cmd = T2CMD_SUBS_LIST;
  hdr = (struct t2cmd_subs_list *) rep->data;
  hdr->tag = tid;

  count = 0;
  h = &d->tag->subscribe_head[0];
  list_for_each_entry(e, h, subscribe[0])
  {
    hdr->subs[count].id = e->id;
    hdr->subs[count].addr = e->addr.sin_addr.s_addr;
    hdr->subs[count].port = e->addr.sin_port;
    hdr->subs[count].flag = 0;
    hdr->subs[count].flag |= e->active? 1:0;
    count ++;
  }

  hdr->count = count;

  rep->len = offsetof(struct t2cmd_subs_list, subs) + count * sizeof(hdr->subs[0]);

  cmd->rl = T2CMD_SIZE(rep);

  return 0;
}

CMD_HANDLER_SETUP(get_tags);
CMD_HANDLER_SETUP(switch_tag);
CMD_HANDLER_SETUP(get_subs);

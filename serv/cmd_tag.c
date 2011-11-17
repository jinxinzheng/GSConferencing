#include "cmd_handler.h"
#include "db/md.h"
#include "devctl.h"

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
  long gid;
  long tid;
  long tuid;

  struct device *d;
  struct tag *t;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(p);

  tid = atol(p);
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

  tuid = TAGUID(gid, tid);

  t = get_tag(tuid);
  if (!t)
  {
    t = tag_create(gid, tid);
  }

  tag_del_device(d->tag, d);

  tag_add_device(t, d);

  d->tag = t;

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

CMD_HANDLER_SETUP(get_tags);
CMD_HANDLER_SETUP(switch_tag);

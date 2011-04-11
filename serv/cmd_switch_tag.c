#include "cmd_handler.h"
#include "db/db.h"

int handle_cmd_switch_tag(struct cmd *cmd)
{
  int a=0;

  char *p;
  long gid;
  long tid;
  long long tuid;

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
    if( dbd = d->db_data )
    {
      dbd->tagid = tid;
      md_update_device(dbd);
    }
  }

  return 0;
}

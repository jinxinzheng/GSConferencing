#include "db/md.h"
#include "sys.h"
#include "devctl.h"
#include <string.h>
#include <arpa/inet.h>
#include "cast.h"
#include "cmd_handler.h"
#include "discuss.h"
#include "ptc.h"

static inline void set_dev_from_data(struct device *d, const struct db_device *data)
{
  d->type = data->type;

  d->addr.sin_family = AF_INET;
  d->addr.sin_addr.s_addr = inet_addr(data->ip);
  d->addr.sin_port = htons(data->port);

  d->fileaddr = d->addr;
  d->fileaddr.sin_port = htons(data->port+1);

  d->bcast.sin_addr.s_addr = 0;
}

static inline void restore_sub(struct device *d)
{
  struct db_device *dd = d->db_data;
  struct tag *t;
  int gid = d->group->id;
  int tid;
  long long tuid;
  int i;

  int subs[] = {dd->sub1, dd->sub2};

  for( i=0 ; i<2 ; i++ )
  {
    if( (tid = subs[i]) )
    {
      tuid = TAGUID(gid, tid);
      t = get_tag(tuid);

      if( !t )
        t = tag_create(gid, tid);

      dev_subscribe(d, t);
    }
  }
}

static inline void restore_discuss(struct device *d)
{
  struct db_device *dd = d->db_data;

  /* detect ptc device */
  if( dd->ptc_id == d->id )
  {
    add_ptc(d);
  }

  if( dd->discuss_open )
  {
    add_open(d->tag, d);

    ptc_put(d);
  }
}

static inline void restore_regist(struct device *d)
{
  d->regist.reg = d->db_data->regist_reg;
}

static inline void restore_vote(struct device *d)
{
  struct db_vote *dv = d->group->vote.current;
  struct vote *v = d->group->vote.v;
  struct db_device *dd = d->db_data;
  int ismember = 0;

  if( !dv )
    return;

  if( dd->vote_master )
  {
    d->vote.v = v;
  }

  if( strcmp(dv->members, "all")==0 )
  {
    ismember = 1;
  }
  else
  {
    char buf[1024];
    char *p;

    strcpy(buf, dv->members);

    IDLIST_FOREACH_p(buf)
    {
      if( d->id == atoi(p) )
      {
        ismember=1;
        break;
      }
    }
  }

  if( ismember )
  {
    vote_add_device(v, d);
    d->vote.v = v;
    ++ v->n_members;

    d->vote.choice = dd->vote_choice;
  }

}

/* recover all devices.
 * groups and tags will be
 * auto created on demand. */
static void recover_devs()
{
  iter it = NULL;
  struct db_device *dd;
  struct device *d;

  add_manager_dev();

  while( (dd = md_iterate_device_next(&it)) )
  {
    d = dev_create(dd->id);

    set_dev_from_data(d, dd);
    d->db_data = dd;

    dev_register(d);

    restore_sub(d);

    restore_discuss(d);

    restore_regist(d);

    restore_vote(d);
  }

  ptc_go_current();
}

void recover_server()
{
  recover_devs();
}

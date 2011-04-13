#include "db/md.h"
#include "sys.h"
#include "devctl.h"

static inline void set_dev_from_data(struct device *d, const struct db_device *data)
{
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
    if( tid = subs[i] )
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

  if( !dd->discuss_open )
    return;

  add_open(d->tag, d);
}

/* recover all devices.
 * groups and tags will be
 * auto created on demand. */
static void recover_devs()
{
  iter it;
  struct db_device *dd;
  struct device *d;

  md_iterate_device_begin(&it);
  while( dd = md_iterate_device_next(&it) )
  {
    d = dev_create(dd->id);

    set_dev_from_data(d, dd);
    d->db_data = dd;

    dev_register(d);

    restore_sub(d);

    restore_discuss(d);
  }
}

void recover_server()
{
  recover_devs();
}

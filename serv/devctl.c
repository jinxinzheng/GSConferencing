#include "sys.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "cast.h"
#include "db/md.h"
#include "group.h"
#include "async.h"
#include <include/types.h>
#include "include/debug.h"
#include <include/util.h>
#include <include/lock.h>

static inline struct device *new_dev()
{
  struct device *d;
  d = (struct device *)malloc(sizeof (struct device));
  memset(d, 0, sizeof (struct device));
  return d;
}

static void def_save_db(struct device *d)
{
  md_update_device(d->db_data);
}

static void def_send_cmd(struct device *d, const char *cmd, int len)
{
  async_sendto_dev(cmd, len, d);
}

static struct dev_ops default_dev_ops = {
  .save_db = def_save_db,
  .send_cmd = def_send_cmd,
};

/* create new dev with default config */
struct device *dev_create(long did)
{
  struct device *d;

  d = new_dev();

  d->ops = &default_dev_ops;

  d->id = did;

  return d;
}

void dev_update_data(struct device *dev)
{
  /* update the device database */
  {
    struct db_device *dbd;
    char *ip = inet_ntoa(dev->addr.sin_addr);
    int port = ntohs(dev->addr.sin_port);

    if( (dbd = md_find_device(dev->id)) )
    {
      dbd->online = 1;
      if (strcmp(dbd->ip, ip) != 0 || dbd->port != port)
      {
        strcpy(dbd->ip, ip);
        dbd->port = port;

        md_update_device(dbd);
      }
    }
    else
    {
      struct db_device tmp = {
        dev->id,
        dev->type,
        "",
        port,
        1, /* tagid */
        "", /* user_id */
        0,"?",1, /* user */
        1, /* online */
        0,0, /* sub */
        0, /* discuss_chair */
        0, /* discuss_open */
        0, /* regist_master */
        0, /* regist_reg */
        0, /* vote_master */
        -1, /* vote_choice */
        -1, "", /* ptc */
      };
      strcpy(tmp.ip, ip);
      /* user_id is defaultly copy of id */
      sprintf(tmp.user_id, "%ld", dev->id);

      md_add_device(&tmp);

      dbd = md_find_device(dev->id);
    }

    dev->db_data = dbd;
  }
}

int dev_register(struct device *dev)
{
  long gid, tid;
  struct group *g;
  struct tag *t;
  long tuid;
  int i;

  if( get_device(dev->id) )
  {
    /* this device has already been registered.
     * this may be due to a reset of the device or some. */
    trace_info("device %ld already registered, ignoring.\n", dev->id);

    /* work around: update tag's broadcast address list.
     * this is the case that the server is restarted.
     * TODO: this work around isn't complete.
     * the addresses are still unknown until the
     * devices registers again.
     * should save this info to db, and recover
     * it with the device. */
    if( dev->tag && dev->bcast.sin_addr.s_addr )
      tag_add_bcast(dev->tag, &dev->bcast);

    return 1;
  }

  gid = 1;
  tid = dev->db_data->tagid;

  /* tag unique id */
  tuid = TAGUID(gid, tid);

  g = get_group(gid);
  if (!g)
  {
    g = group_create(gid);
  }

  group_add_device(g, dev);

  t = get_tag(tuid);
  if (!t)
  {
    t = tag_create(gid, tid);
  }

  tag_add_device(t, dev);

  if (dev->bcast.sin_addr.s_addr)
    tag_add_bcast(t, &dev->bcast);

  dev->group = g;
  dev->tag = t;

  for( i=0 ; i<MAX_SUB ; i++ )
  {
    dev->subscription[i] = NULL;
    INIT_LIST_HEAD(&dev->subscribe[i]);
  }

  /* an fifo of 32 in length */
  cfifo_init(&dev->pack_fifo, 5, 2);

  add_device(dev);

  trace_info("device %ld registered\n", dev->id);

  if( dev->db_data->sub1 == 0 &&
      dev->db_data->sub2 == 0 )
  {
    /* any client default subscribes to tag 1 */
    t = get_tag(TAGUID(gid, 1));
    if( !t )
      t = tag_create(gid, 1);
    dev_subscribe(dev, t);
  }

  /* update group stats and caches */
  if( dev->type < array_size(g->stats.dev_count) )
  {
    g->stats.dev_count[dev->type] ++;
  }

  append_dev_ents_cache(g, dev);

  return 0;
}

int dev_unregister(struct device *dev)
{
  struct tag *t;
  int i;

  for( i=0 ; i<MAX_SUB ; i++ )
  {
    if( dev->subscription[i] )
    {
      list_del(&dev->subscribe[i]);
      dev->subscription[i] = NULL;
    }
  }

  if ((t = dev->tag) != NULL)
  {
    list_del(&dev->tlist);
  }

  if (dev->group)
  {
    list_del(&dev->list);
  }

  return 0;
}

void device_save(struct device *d)
{
  if( d->ops->save_db )
  {
    d->ops->save_db(d);
  }
}

/* down cast to device */
void device_cmd(struct device *d, const char *cmd, int len)
{
  if( d->ops->send_cmd )
  {
    d->ops->send_cmd(d, cmd, len);
  }
}

#if 0
/* passed up to server */
int dev_event(struct device *dev, int event, void *args)
{
  /* invoke event handler */
  return 0;
}
#endif

/* manager dev is the dev with id 0 and
 * only exists in runtime, never saved
 * to the database. */
void add_manager_dev()
{
  int id = 0;
  static struct device d;
  static struct db_device dd;

  static struct dev_ops ops = {
    .save_db = NULL,
    .send_cmd = NULL,
  };

  memset(&dd, 0, sizeof (struct db_device));
  dd.id = id;
  dd.tagid = 1;

  memset(&d, 0, sizeof (struct device));
  d.ops = &ops;
  d.id = id;
  d.type = DEVTYPE_NONE;

  d.db_data = &dd;

  dev_register(&d);
}


static void do_dev_activate(struct device *d, int a)
{
  struct group *g = d->group;

  LOCK(g->stats_lk);

  if( d->active == a )
    goto end;

  d->active = a;

  if( d->type < array_size(g->stats.active_count) )
  {
    if( a )
      ++ g->stats.active_count[d->type];
    else
      -- g->stats.active_count[d->type];
  }

  dirty_dev_ents_cache(g);

end:
  UNLOCK(g->stats_lk);
}

void dev_activate(struct device *d)
{
  do_dev_activate(d, 1);
}

void dev_deactivate(struct device *d)
{
  do_dev_activate(d, 0);
}

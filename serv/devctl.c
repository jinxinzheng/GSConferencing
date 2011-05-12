#include "sys.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "cast.h"
#include "db/md.h"
#include "group.h"
#include "include/debug.h"

struct device *dev_create(long did)
{
  struct device *d;

  d = (struct device *)malloc(sizeof (struct device));
  memset(d, 0, sizeof (struct device));

  d->id = did;

  return d;
}

void dev_update_data(struct device *dev)
{

  dev->active = 1;

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
        "",
        port,
        1, /* tagid */
        0,"?",1, /* user */
        1, /* online */
        0,0, /* sub */
        0, /* discuss_chair */
        0, /* discuss_open */
        0, /* regist_master */
        0, /* regist_reg */
        0, /* vote_master */
        -1, /* vote_choice */
        0, "", /* ptc */
      };
      strcpy(tmp.ip, ip);

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
  long long tuid;
  int i;

  if( get_device(dev->id) )
  {
    /* this device has already been registered.
     * this may be due to a reset of the device or some. */
    trace_info("device %ld already registered, ignoring.\n", dev->id);
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

  /* 32 elements of 4 bytes in fifo.
   * if the fifo sends faster than and lagged by other slow devs,
   * the bufs will be piled at the faster fifo.
   * if the piled bufs reaches the max 32, they will be auto
   * discarded. this could eliminate the lagging, but may
   * produce noticible voice abruption. */
  cfifo_init(&dev->pack_fifo, 5, 2);
  cfifo_enable_locking(&dev->pack_fifo);

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
  md_update_device(d->db_data);
}

/* down cast to device */
int dev_control(struct device *dev, int ctl, void *params)
{
  return 0;
}

/* passed up to server */
int dev_event(struct device *dev, int event, void *args)
{
  /* invoke event handler */
  return 0;
}

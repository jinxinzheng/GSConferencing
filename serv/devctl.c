#include "sys.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "cast.h"
#include "db/md.h"

struct group *group_create(long gid)
{
  struct group *g;

  g = (struct group *)malloc(sizeof (struct group));
  memset(g, 0, sizeof(struct group));

  g->id = gid;
  INIT_LIST_HEAD(&g->device_head);
  add_group(g);

  printf("group %ld created\n", gid);
  return g;
}

int dev_register(struct device *dev)
{
  long gid, tid;
  struct group *g;
  struct tag *t;
  long long tuid;

  struct device *d;

  if (d = get_device(dev->id))
  {
    /* this device has already been registered.
     * this may be due to a reset of the device or some. */
    printf("device %ld already registered, ignoring.\n", dev->id);
    /* should update the properties of the device like the address */
    d->addr = dev->addr;
    d->fileaddr = d->fileaddr;
    return 1;
  }

  dev->active = 1;

  /* update the device database and
   * set up group, tag */
  {
    struct db_device *dbd;
    char *ip = inet_ntoa(dev->addr.sin_addr);
    int port = ntohs(dev->addr.sin_port);

    gid = 1;
    if (dbd = md_find_device(dev->id))
    {
      dbd->online = 1;
      if (strcmp(dbd->ip, ip) != 0 || dbd->port != port)
      {
        strcpy(dbd->ip, ip);
        dbd->port = port;

        md_update_device(dbd);
      }

      tid = dbd->tagid;
    }
    else
    {
      struct db_device tmp = {
        dev->id,
        "",
        port,
        1,
        0,"?",1,
        1
      };
      strcpy(tmp.ip, ip);

      md_add_device(&tmp);

      tid = tmp.tagid;
    }
  }

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
  dev->subscription = NULL;
  INIT_LIST_HEAD(&dev->subscribe);

  /* 32 elements of 4 bytes in fifo.
   * if the fifo sends faster than and lagged by other slow devs,
   * the bufs will be piled at the faster fifo.
   * if the piled bufs reaches the max 32, they will be auto
   * discarded. this could eliminate the lagging, but may
   * produce noticible voice abruption. */
  cfifo_init(&dev->pack_fifo, 5, 2);
  cfifo_enable_locking(&dev->pack_fifo);

  add_device(dev);

  printf("device %ld registered\n", dev->id);

  /* any client is initially subscribed to tag 1 */
  t = get_tag(TAGUID(gid, 1));
  if( !t )
    t = tag_create(gid, 1);
  dev_subscribe(dev, t);

  return 0;
}

int dev_unregister(struct device *dev)
{
  struct tag *t;

  if (dev->subscription)
  {
    list_del(&dev->subscribe);
  }

  if ((t = dev->tag) != NULL)
  {
    list_del(&dev->tlist);
  }

  if (dev->group)
  {
    list_del(&dev->list);
  }
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

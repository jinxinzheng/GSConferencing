#include "sys.h"
#include <stdlib.h>
#include <stdio.h>
#include "cast.h"

struct group *group_create(long gid)
{
  struct group *g;

  g = (struct group *)malloc(sizeof (struct group));
  g->id = gid;
  INIT_LIST_HEAD(&g->device_head);
  add_group(g);

  printf("group %ld created\n", gid);
  return g;
}

struct tag *tag_create(long gid, long tid)
{
  struct tag *t;
  pthread_t thread;
  long long tuid;

  tuid = TAGUID(gid, tid);

  t = (struct tag *)malloc(sizeof (struct tag));
  t->tid = tid;
  t->id = tuid;
  t->name[0]=0;
  t->device_list = NULL;
  INIT_LIST_HEAD(&t->subscribe_head);
  blocking_queue_init(&t->pack_queue);
  add_tag(t);

  /* create the tag's casting queue thread */
  pthread_create(&thread, NULL, tag_run_casting, t);

  printf("tag %ld:%ld created\n", gid, tid);
  return t;
}

int dev_register(struct device *dev)
{
  long gid, tid;
  struct group *g;
  struct tag *t;
  long long tuid;

  if (get_device(dev->id))
  {
    /* this device has already been registered.
     * this may be due to a reset of the device or some. */
    printf("device %ld already registered, ignoring.\n", dev->id);
    /* maybe we should update the properties of the device like the address? */
    return 1;
  }

  /* set up group,tag from database */
  gid=1;
  tid = dev->id / 100;

  /* tag unique id */
  tuid = TAGUID(gid, tid);

  g = get_group(gid);
  if (!g)
  {
    g = group_create(gid);
  }

  t = get_tag(tuid);
  if (!t)
  {
    t = tag_create(gid, tid);

    t->device_list = &dev->list;

    /* for a new tag, this is when the device is inserted into the group link */
    group_add_device(g, dev);
  }
  else
  {
    if (!t->device_list) {
      t->device_list = &dev->list;
      /* insert device into group */
      group_add_device(g, dev);
    }
    else
      /* this will also insert device into group */
      tag_add_device(t, dev);
  }

  dev->group = g;
  dev->tag = t;
  dev->subscription = NULL;

  add_device(dev);

  printf("device %ld registered\n", dev->id);

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
    struct list_head *p;
    p = &dev->list;
    if (p == t->device_list)
    {
      p = p->next;
      if (
          (p == &dev->group->device_head) /* head is not a real entry*/ ||
          ((list_entry(p, struct device, list))->tag != t) /*the deleting device is the only one under this tag*/
         )
        p = NULL;

      t->device_list = p;
    }
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
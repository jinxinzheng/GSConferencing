#include "sys.h"
#include <stdlib.h>
#include <stdio.h>
#include "cast.h"

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
    g = (struct group *)malloc(sizeof (struct group));
    g->id = gid;
    INIT_LIST_HEAD(&g->device_head);
    add_group(g);

    printf("group %ld created\n", gid);
  }

  t = get_tag(tuid);
  if (!t)
  {
    pthread_t thread;
    t = (struct tag *)malloc(sizeof (struct tag));
    t->tid = tid;
    t->id = tuid;
    t->name[0]=0;
    t->device_list = &dev->list;
    INIT_LIST_HEAD(&t->subscribe_head);
    blocking_queue_init(&t->pack_queue);
    add_tag(t);

    /* for a new tag, this is when the device is inserted into the group link */
    group_add_device(g, dev);

    /* create the tag's casting queue thread */
    pthread_create(&thread, NULL, tag_run_casting, t);

    printf("tag %ld:%ld created\n", gid, tid);
  }
  else
  {
    if (!t->device_list)
      t->device_list = &dev->list;
    else
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

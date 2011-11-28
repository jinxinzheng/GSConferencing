#include "sys.h"
#include "hash.h"
#include <string.h>

static struct group *group_hash[HASH_SZ];
static struct tag *tag_hash[HASH_SZ];
static struct device *device_hash[HASH_SZ];

int init_groups()
{
  memset(group_hash, 0, sizeof group_hash);
  memset(tag_hash, 0, sizeof tag_hash);
  memset(device_hash, 0, sizeof device_hash);
  return 0;
}

struct group *get_group(long gid)
{
  return find_by_id(group_hash, gid);
}

void add_group(struct group *g)
{
  hash_id(group_hash, g);
}


/* don't use tag.id to hash tags.
 * it's not unique. use the unique tuid. */
struct tag *get_tag(long id)
{
  return find_by_memb(tag_hash, tuid, id);
}

void add_tag(struct tag *t)
{
  hash_memb(tag_hash, tuid, t);
}


struct device *get_device(long did)
{
  return find_by_id(device_hash, did);
}

void add_device(struct device *dev)
{
  hash_id(device_hash, dev);
}

/* the device is added to the tail of the group link. */
void group_add_device(struct group *g, struct device *dev)
{
  list_add_tail(&dev->list, &g->device_head);
}

/* add to the tail of the tag link. */
void tag_add_device(struct tag *t, struct device *dev)
{
  list_add_tail(&dev->tlist, &t->device_head);
}

void tag_del_device(struct device *d)
{
  list_del(&d->tlist);
}

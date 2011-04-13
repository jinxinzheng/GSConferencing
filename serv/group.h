#ifndef __GROUP_H__
#define __GROUP_H__

#include "include/list.h"

struct db_group;
struct db_discuss;

struct device;

struct group {
  long id;

  struct db_group *db_data;

  //struct device *device_head;
  struct list_head device_head;

  /* ID hash table linkage. */
  struct group *hash_next;
  struct group **hash_pprev;

  struct device *chairman;

  struct {
    struct db_discuss *current;
    int disabled;
  } discuss;

  struct {
    int expect;
    int arrive;
  } regist;
};


struct group *group_create(long gid);

void group_save(struct group *g);

#endif  /*__GROUP_H__*/

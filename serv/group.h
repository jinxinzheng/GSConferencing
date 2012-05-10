#ifndef __GROUP_H__
#define __GROUP_H__

#include "include/list.h"

struct db_group;
struct db_discuss;
struct db_vote;

struct device;

struct group {
  int id;

  struct db_group *db_data;

  //struct device *device_head;
  struct list_head device_head;

  /* ID hash table linkage. */
  struct group *hash_next;
  struct group **hash_pprev;

  struct device *cyctl;

  struct device *chairman;

  struct {
    int dev_count[8];
    int active_count[8];
  } stats;
  pthread_mutex_t stats_lk;

  struct {
    struct db_discuss *current;
    int curr_num;
    int nmembers;
    int memberids[1024];
    char *membernames;
    int membernames_size;
    int disabled;
  } discuss;

  struct {
    int mode;
    int memberids[1024];
    int nmembers;
    int expect;
    int arrive;
    int arrive_ids[1024];
  } regist;

  struct {
    struct db_vote *current;
    int curr_num;
    int expect;
    int nvoted;
    int memberids[1024];
    int nmembers;
    char *membernames;
    int membernames_size;
    struct vote *v; /* for recover use */
  } vote;

  struct {
    char *dev_ents;
    int dev_ents_size;
    int dev_ents_len;
    int dev_ents_dirty;
  } caches;
};


struct group *group_create(int gid);

void group_save(struct group *g);


void append_dev_ents_cache(struct group *g, struct device *d);

void refresh_dev_ents_cache(struct group *g);

static inline void dirty_dev_ents_cache(struct group *g)
{
  g->caches.dev_ents_dirty = 1;
}

static inline int is_dirty_dev_ents_cache(struct group *g)
{
  return g->caches.dev_ents_dirty;
}


void group_setup_discuss(struct group *g, struct db_discuss *s);

void group_setup_vote(struct group *g, struct db_vote *dv);


#endif  /*__GROUP_H__*/

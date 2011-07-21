#ifndef __GROUP_H__
#define __GROUP_H__

#include "include/list.h"

struct db_group;
struct db_discuss;
struct db_vote;

struct device;

struct group {
  long id;

  struct db_group *db_data;

  //struct device *device_head;
  struct list_head device_head;

  /* ID hash table linkage. */
  struct group *hash_next;
  struct group **hash_pprev;

  struct device *cyctl;

  struct device *chairman;

  struct {
    struct db_discuss *current;
    int curr_num;
    int nmembers;
    int memberids[1024];
    char membernames[2048];
    int disabled;
  } discuss;

  struct {
    int expect;
    int arrive;
  } regist;

  struct {
    struct db_vote *current;
    int curr_num;
    int nmembers;
    int nvoted;
    int memberids[1024];
    char membernames[2048];
    struct vote *v; /* for recover use */
  } vote;
};


struct group *group_create(long gid);

void group_save(struct group *g);



void group_setup_discuss(struct group *g, struct db_discuss *s);

void group_setup_vote(struct group *g, struct db_vote *dv);


#endif  /*__GROUP_H__*/

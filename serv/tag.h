#ifndef _TAG_H_
#define _TAG_H_

#include "include/list.h"
#include "include/cfifo.h"

struct device;

struct tag {
  long tid;
  /*as tid is not unique, need to use group_id<<16 + tag_id as the unique id. */
  long long id;
  char name[64];

  /* the socket used for casting */
  int sock;

  /* tagged device.tlist */
  struct list_head device_head;

  /* see device.subscription */
  //struct device *subscribe_head;
  struct list_head subscribe_head;

  /* ID hash table linkage. */
  struct tag *hash_next;
  struct tag **hash_pprev;

  /* support up to 8 devices mixing audio */
  struct device *mix_devs[8];
  int mix_count;
  unsigned int mix_mask;
  unsigned int mix_stat;

  pthread_mutex_t mut;
  pthread_cond_t  cnd_nonempty;

  /* broadcast addresses, pointers to device.bcast.
   * support up to 64 */
  struct sockaddr_in *bcasts[64];
  int bcast_size;
};

#define TAGUID(gid,tid) (((long long)gid<<32) ^ tid)

struct tag *tag_create(long gid, long tid);

struct packet;

void tag_add_bcast(struct tag *t, struct sockaddr_in *bcast);

void tag_in_dev_packet(struct tag *t, struct device *d, struct packet *pack);

struct packet *tag_out_dev_mixed(struct tag *t);

#endif

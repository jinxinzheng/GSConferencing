#ifndef _TAG_H_
#define _TAG_H_

#include "include/list.h"
#include "include/cfifo.h"

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

  /* packet fifo */
  struct cfifo pack_fifo;

  /* broadcast addresses, pointers to device.bcast.
   * support up to 64 */
  struct sockaddr_in *bcasts[64];
  int bcast_size;
};

#define TAGUID(gid,tid) (((long long)gid<<32) ^ tid)

struct tag *tag_create(long gid, long tid);

struct packet;

void tag_enque_packet(struct tag *t, struct packet *p);

struct packet *tag_deque_packet(struct tag *t);

void tag_add_bcast(struct tag *t, struct sockaddr_in *bcast);

#endif

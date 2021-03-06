#ifndef _TAG_H_
#define _TAG_H_

#include "include/list.h"
#include <include/queue.h>
#include <pthread.h>
#include <include/cfifo.h>

struct device;

struct packet;

struct mixer;

#define MAX_SUB 1
#define MAX_MIX 8

struct tag {
  int id;
  /*as id is not unique, need to use group_id<<16 + tag_id as the unique id. */
  int tuid;
  char name[64];

  /* the socket used for casting */
  int sock;

  /* tagged device.tlist */
  struct list_head device_head;

  /* see device.subscription */
  struct list_head subscribe_head[MAX_SUB];

  /* ID hash table linkage. */
  struct tag *hash_next;
  struct tag **hash_pprev;

  struct mixer *mixer;

  /* support up to 8 devices mixing audio */
  struct device *mix_devs[MAX_MIX];
  int mix_count;
  int mix_ref;
  int mix_silence;

  pthread_mutex_t mut;
  pthread_cond_t  cnd_nonempty;


  struct {
#define REP_CAST_SIZE (1<<4)
#define REP_CAST_MASK (REP_CAST_SIZE-1)
    struct packet *rep_pack[REP_CAST_SIZE];
    int rep_pos;
    struct cfifo rep_reqs;
    unsigned int seq;
  } cast;

  struct {
    int mode;
    struct list_head open_list;
    int maxuser;
    int openuser;
    pthread_mutex_t lk;
  } discuss;

  struct {
    int mode;
    /* rep tag */
    struct tag *rep;
  } interp;

  /* broadcast addresses, pointers to device.bcast.
   * support up to 64 */
  struct sockaddr_in *bcasts[64];
  int bcast_size;

  struct device *ucast;
};

#define TAGUID(gid,tid) ((gid<<16) ^ tid)

#define TAG_GETGID(tag) ( ((tag->tuid)>>16)&0xffff )

struct tag *request_tag(int gid, int tid);

struct tag *tag_create(int gid, int tid);

int tag_check_ucast(struct tag *t, struct device *d);

void tag_add_bcast(struct tag *t, struct sockaddr_in *bcast);

int tag_in_dev_packet(struct tag *t, struct device *d, struct packet *pack);

struct packet *tag_out_dev_mixed(struct tag *t);

void tag_add_outstanding(struct tag *t, struct device *d);

void tag_rm_outstanding(struct tag *t, struct device *d);

#endif

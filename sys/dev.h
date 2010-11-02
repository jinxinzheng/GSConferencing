#ifndef _DEV_H_
#define _DEV_H_

#include "list.h"
#include "queue.h"
#include <sys/types.h>
#include <netinet/in.h>
#include "vote.h"

enum {
  DEVTYPE_NONE
};

enum {
  MODE_NONE
};

/* the device linked list design:
 * (d for device, t for tag, g for group)
 *
 * g
 * |
 * d - d - d - d - d - d - ... - d
 * |           |   |
 * t           t   t
 *
 * all devices of a group are placed in a 'big linked list'.
 * devices with the same tag are arranged adjacent to each other in the list.
 *
 * */

struct device;

struct packet
{
  void *data;
  size_t len;
  struct device *dev; /* by what device it was sent */

  struct list_head l;
};

struct tag {
  long tid;
  /*as tid is not unique, need to use group_id<<16 + tag_id as the unique id. */
  long long id;
  char name[64];

  /* the socket used for casting */
  int sock;

  /* tagged devices
   * this list is not stand-alone. it's partial of the group list.
   * so can't use a real head here, just use a link pointer. */
  struct list_head *device_list;

  /* see device.subscription */
  //struct device *subscribe_head;
  struct list_head subscribe_head;

  /* ID hash table linkage. */
  struct tag *hash_next;
  struct tag **hash_pprev;

  /* packet queue */
  struct blocking_queue pack_queue;
};

#define TAGUID(gid,tid) (((long long)gid<<32) ^ tid)

struct group {
  long id;

  //struct device *device_head;
  struct list_head device_head;

  /* ID hash table linkage. */
  struct group *hash_next;
  struct group **hash_pprev;
};

struct device {
  long id;
  /*unsigned long ip;
  long port;*/
  struct sockaddr_in addr;
  int type;
  int mode;
  int privilege;
  int active :1;

  /* group and tag is stored in database */
  struct group *group;

  struct tag *tag;

  /* list layout (designed for tagged device iteration):
   * tag0-tag0-tag0 - tag1-tag1 - tag2 -... */
  /*struct device *prev,*next;*/
  struct list_head list;

  /* used for cast. currently supports only one tag */
  struct tag *subscription;

  struct list_head subscribe;

  /* ID hash table linkage. */
  struct device *hash_next;
  struct device **hash_pprev;

  struct {
    struct vote *v;
    struct list_head l;
    int choice;
  } vote;
};

#endif

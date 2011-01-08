#ifndef _DEV_H_
#define _DEV_H_

#include "include/list.h"
#include "include/queue.h"
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

#include "tag.h"

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
  struct sockaddr_in bcast;
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
    int forbidden;
  } vote;

  struct {
    struct list_head l;
    int open;
    int forbidden;
  } discuss;
};

#endif

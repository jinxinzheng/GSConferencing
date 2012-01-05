#ifndef _DEV_H_
#define _DEV_H_

#include "include/list.h"
#include "include/queue.h"
#include <sys/types.h>
#include <netinet/in.h>
#include "vote.h"

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
 *
 * */

#include "group.h"
#include "tag.h"

struct db_device;
struct device;

struct dev_ops
{
  void (*save_db)(struct device *d);
  void (*send_cmd)(struct device *d, const char *cmd, int len);
};

struct device
{
  struct dev_ops *ops;

  long id;
  /*unsigned long ip;
  long port;*/
  struct sockaddr_in addr;
  /* the address which receives file */
  struct sockaddr_in fileaddr;
  struct sockaddr_in bcast;
  int type;
  int mode;
  int privilege;
  int active;

  struct db_device *db_data;

  int audio_sock;
  int audio_bad;

  int hbeat;

  /* group and tag is stored in database */
  struct group *group;

  struct tag *tag;

  /* list in group */
  struct list_head list;

  /* list in tag */
  struct list_head tlist;


  /* used for cast. currently supports only one tag */
  struct tag *subscription[MAX_SUB];

  struct list_head subscribe[MAX_SUB];

  /* ID hash table linkage. */
  struct device *hash_next;
  struct device **hash_pprev;

  /* packet queue */
  struct cfifo pack_fifo;
  /* used by the tag mixing code */
  unsigned int mixbit;
  int timeouts;
  int highload;

  struct {
    int res;
    const char *info;
  } mixer;

  struct {
    int flushed;
  } stats;

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

  struct {
    int reg;
  } regist;

  struct {
    struct list_head l;
  } ptc;
};

#endif

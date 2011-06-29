#ifndef _PACKET_H_
#define _PACKET_H_

#include "dev.h"

/* not exceeding 2K */
#define MAXPACK 1800

struct packet
{
  struct device *dev; /* by what device it was sent */

  struct list_head queue_l; /* queue */

  size_t len; /* length of data */
  char data[MAXPACK];
};

void init_pack_pool();

struct packet *pack_get_new();

void pack_free(struct packet *p);

struct packet *pack_dup(struct packet *p);

#endif

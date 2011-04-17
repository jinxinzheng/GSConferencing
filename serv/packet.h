#ifndef _PACKET_H_
#define _PACKET_H_

#include "dev.h"

/* not exceeding a page */
#define MAXPACK 4000

struct packet
{
  struct list_head free; /* free list */

  struct device *dev; /* by what device it was sent */

  struct list_head l; /* queue */

  size_t len; /* length of data */
  char data[MAXPACK];
};

struct packet *pack_get_new();

void pack_free(struct packet *p);

struct packet *pack_dup(struct packet *p);

#endif

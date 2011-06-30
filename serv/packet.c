#include "packet.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "block.h"
#include "include/debug.h"

/* packet objects are pooled/recycled
 * to improve performance.
 * note: we can't use fifo because there
 * are more than one writer. */
static struct block_pool *free_pool;

void init_pack_pool()
{
  /* each block size is bigger than MAXPACK
   * and up-rounded to smallest power-of-2. */

  int i = 0;
  int m = MAXPACK;
  do
  {
    i++;
  }
  while( (m >>= 1) );

  /* alloc 2K*1024=2M memory at once and limited to this size.
   * this should be adequate. there are at most 8 queues and
   * each queue has maximumly 32 packets. theorectically 256
   * packets are in use at most. allocing 1024 helps in
   * performance. */
  free_pool = init_block_pool(1<<i, 1024, 1);
}

struct packet *pack_get_new()
{
  struct packet *p;

  while( !(p = (struct packet *) alloc_block(free_pool)) )
  {
    trace_warn("out of packet resource!\n");
    /* wait for a while for the resource to be freed */
    usleep(10*1000);
  }

  return p;
}

void pack_free(struct packet *p)
{
  free_block(free_pool, p);
}

struct packet *pack_dup(struct packet *p)
{
  struct packet *newp = pack_get_new();

  memcpy(newp->data, p->data, p->len);
  newp->len = p->len;

  return newp;
}

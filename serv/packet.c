#include "packet.h"
#include <stdlib.h>
#include <string.h>
#include "block.h"

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

  free_pool = init_block_pool(1<<i, 256, 0);
}

struct packet *pack_get_new()
{
  struct packet *p;

  /* we are never empty */
  p = (struct packet *) alloc_block(free_pool);

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

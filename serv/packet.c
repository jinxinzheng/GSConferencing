#include "packet.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "block.h"
#include "include/debug.h"

//#define PACK_DEBUG
#ifdef  PACK_DEBUG
#define PACK_DBG(a...) fprintf(stderr, "packet: " a)
#else
#define PACK_DBG(...)
#endif

#define PACK_SHIFT  11
#define PACK_SIZE   (1<<PACK_SHIFT)
#define PACK_COUNT  1024
#define COUNT_MASK  (PACK_COUNT-1)

/* packet objects are pooled/recycled
 * to improve performance.
 * note: we can't use fifo because there
 * are more than one writer. */
static struct block_pool *free_pool;

/* the fast lockless cache */
static char *cache_base;
static char cache_use[PACK_COUNT] = {0};
static int cache_pos = 0;
#define CACHE_SIZE        (PACK_COUNT<<PACK_SHIFT)

#define PACK_IS_CACHE(p)  \
  ((char *)p > cache_base && (char *)(p) < cache_base+CACHE_SIZE )

#define CACHE_OFFSET(p)   (((char*)(p) - cache_base) >> PACK_SHIFT)

#define CACHE_GET(off)    ((struct packet *)(cache_base + ((off)<<PACK_SHIFT)))

void init_pack_pool()
{
  /* alloc 2K*1024=2M memory at once and limited to this size.
   * this should be adequate. there are at most 8 queues and
   * each queue has maximumly 32 packets. theorectically 256
   * packets are in use at most. allocing 1024 helps in
   * performance. */
  free_pool = init_block_pool(PACK_SIZE, PACK_COUNT, 1);

  /* alloc the lockless cache */
  cache_base = (char *)malloc(CACHE_SIZE);
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

static void pack_free_cache(struct packet *p);

void pack_free(struct packet *p)
{
  if( PACK_IS_CACHE(p) )
  {
    pack_free_cache(p);
  }
  else
  {
    free_block(free_pool, p);
  }
}

/* designed to be safe to call in multiple threads. */
static void pack_free_cache(struct packet *p)
{
  int i;
  i = CACHE_OFFSET(p);
  PACK_DBG("free cache %d\n", i);
  cache_use[i] = 0;
  /* make next pack_get_cache() hit. */
  cache_pos = i;
}

static struct packet *pack_get_cache()
{
  int pos;
  int i;

  /* take a shot of current pos */
  pos = cache_pos;

  if( !cache_use[pos] )
  {
    /* hit */
    /* try to make next call hit more possible. */
    cache_pos = (pos+1)&COUNT_MASK;
    cache_use[pos] = 1;
    PACK_DBG(" get cache %d (hit)\n", pos);
    return CACHE_GET(pos);
  }
  else
  {
    /* find a free block */
    for ( i=(pos+1)&COUNT_MASK;
          i!=pos;
          i=(i+1)&COUNT_MASK )
    {
      if( !cache_use[i] )
      {
        cache_pos = (i+1)&COUNT_MASK;
        cache_use[i] = 1;
        PACK_DBG(" get cache %d (find)\n", pos);
        return CACHE_GET(i);
      }
    }
  }

  return NULL;
}

/* get from the lockless cache.
 * must be used in only 1 thread to ensure safety. */
struct packet *pack_get_fast()
{
  struct packet *p;
  while( !(p = pack_get_cache()) )
  {
    trace_warn("out of packet cache!\n");
    /* wait for a while for the cache to be freed */
    usleep(10*1000);
  }
  return p;
}

struct packet *pack_dup(struct packet *p)
{
  struct packet *newp = pack_get_new();

  memcpy(newp->data, p->data, p->len);
  newp->len = p->len;

  return newp;
}

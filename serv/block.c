/* pooled buffers that have same sizes.
 * safe to be used for multiple threads.
 * */
#include "block.h"
#include <stdlib.h>
#include <pthread.h>
#include <include/debug.h>

//#define BLOCK_DEBUG
#ifdef  BLOCK_DEBUG
#define BLK_DBG(a...) fprintf(stderr, "block: " a)
#else
#define BLK_DBG(...)
#endif

struct block_pool
{
  struct list_head free_list;
  pthread_mutex_t m;
  int bsize; /* block size */
  int count; /* total count for blocks ever alloced */
  int limited; /* is resource limited to initial count? */
};

struct block
{
  struct list_head free;

  unsigned char data[1];
};

#define block_data(b) \
  ((b)->data)

#define block_entry(buf) \
  ((struct block *)((char *)(buf)-(unsigned long)(&((struct block *)0)->data)))

struct block_pool *init_block_pool(int block_size, int init_count, int limited)
{
  struct block_pool *pool = (struct block_pool *)malloc(sizeof(struct block_pool));

  BLK_DBG("init block pool: block size %d, count %d, at %p\n", block_size, init_count, pool);

  INIT_LIST_HEAD(&pool->free_list);
  pthread_mutex_init(&pool->m, NULL);
  pool->bsize = block_size;
  pool->count = init_count;
  pool->limited = limited;

  if( init_count > 0 )
  {
    char *buf = calloc(init_count, block_size);
    struct block *b;
    int i;

    for( i=0 ; i<init_count ; i++ )
    {
      b = (struct block *) (buf + i * block_size);
      list_add_tail(&b->free, &pool->free_list);
    }
  }

  return pool;
}

void *alloc_block(struct block_pool *bp)
{
  struct block *b;

  pthread_mutex_lock(&bp->m);

  if (list_empty(&bp->free_list))
  {
    b = NULL;
  }
  else
  {
    struct list_head *l;

    l = bp->free_list.next;
    b = list_entry(l, struct block, free);
    BLK_DBG("get block at pool %p. block=%p, next=%p, prev=%p\n",
      bp, b, l->next, l->prev);
    list_del(l);
  }

  pthread_mutex_unlock(&bp->m);

  if( !b )
  {
    if( bp->limited )
      return NULL;
    else
    {
      int c;
      b = (struct block *) malloc(bp->bsize);
      c = ++ bp->count;
      if( (c & ((1<<9)-1)) == 0 )
      {
        trace_warn("hitting %d blocks\n", c);
      }
    }
  }

  return block_data(b);
}

void free_block(struct block_pool *bp, void *buf)
{
  struct block *b = block_entry(buf);
  BLK_DBG("free block to pool %p. block=%p, next=%p, prev=%p\n",
    bp, b, b->free.next, b->free.prev);

  pthread_mutex_lock(&bp->m);

  list_add(&b->free, &bp->free_list);

  pthread_mutex_unlock(&bp->m);
}

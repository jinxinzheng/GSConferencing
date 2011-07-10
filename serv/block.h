#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <include/list.h>

struct block_pool;

/* init block pool.
 * if limited=1, then only init_count of
 * blocks are available. If all blocks
 * are in use, then alloc_block() will fail.
 * */
struct block_pool *init_block_pool(int block_size, int init_count, int limited);

/* allocate a block from the pool.
 * returns NULL on failure.
 * */
void *alloc_block(struct block_pool *bp);

void free_block(struct block_pool *bp, void *buf);

#endif  /*__BLOCK_H__*/

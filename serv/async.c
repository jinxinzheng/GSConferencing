/* do jobs asynchronously using thread pool.
 * the async methods *could* block if we are
 * out of resources.
 * */

#include <threadpool/threadpool.h>
#include "block.h"
#include "dev.h"
#include "network.h"
#include <string.h>
#include <unistd.h>
#include <include/debug.h>

#define THREADS 32

static threadpool tp;
static struct block_pool *bp;

void async_init()
{
  /* kick some threads off */
  tp = create_threadpool(THREADS);
  bp = init_block_pool(2048, THREADS, 1);
}

struct _send_args
{
  struct device *d;
  int len;
  unsigned char buf[1];
};

static void _sendto_dev(void *arg)
{
  struct _send_args *args = (struct _send_args *)arg;

  sendto_dev_tcp(args->buf, args->len, args->d);

  free_block(bp, arg);
}

void async_sendto_dev(const void *buf, int len, struct device *d)
{
  struct _send_args *args;

  while( !(args = (struct _send_args *) alloc_block(bp)) )
  {
    trace_warn("all pooled thread in use. wait for a while..\n");
    /* wait for 100ms to allow some other thread to finish. */
    usleep(100*1000);
  }

  args->d = d;
  args->len = len;
  memcpy(args->buf, buf, len);

  dispatch_threadpool(tp, _sendto_dev, args);
}

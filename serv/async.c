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
#include <include/util.h>
#include <include/lock.h>

#define THREADS 128
#define BLKSZ   10240 /* conform to CMD_MAX */

static threadpool tp;
static struct block_pool *bp;

void async_init()
{
  /* kick some threads off */
  tp = create_threadpool(THREADS);
  bp = init_block_pool(BLKSZ, THREADS, 1);
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

  if( !d->active )
  {
    /* optimized for avoiding thread pool exhausting  */
    if( d->id != 1) /* work around for the manager special dev.. */
      return;
  }

  if( len > BLKSZ-100 )
  {
    trace_err("can't dispatch the cmd is too long: %d\n", len);
    return;
  }

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

struct brcast_args
{
  int sock;
  int repeat;
  int len;
  unsigned char data[1];
};

static pthread_mutex_t brcast_lk = PTHREAD_MUTEX_INITIALIZER;

static void run_brcast(void *arg)
{
  struct brcast_args *args = (struct brcast_args *) arg;
  int i;
  for( i=0 ; i<args->repeat ; i++ )
  {
    /* the socket can be used in multiple threads. */
    LOCK(brcast_lk);
    broadcast_local(args->sock, args->data, args->len);
    UNLOCK(brcast_lk);
    sleep(2);
  }
  free_block(bp, arg);
}

void async_brcast(int sock, const void *buf, int len, int repeat)
{
  struct brcast_args *args;

  if( len > BLKSZ-100 )
  {
    trace_err("broadcast data is too long: %d\n", len);
    return;
  }

  while( !(args = (struct brcast_args *) alloc_block(bp)) )
  {
    msleep(100);
  }

  args->sock = sock;
  args->repeat = repeat;
  args->len = len;
  memcpy(args->data, buf, len);

  dispatch_threadpool(tp, run_brcast, args);
}

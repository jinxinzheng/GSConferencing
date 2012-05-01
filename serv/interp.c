#include "include/pack.h"
#include "tag.h"
#include "network.h"
#include <include/lock.h>
#include "async.h"

#define REPEAT  10

static int sock = 0;
static pthread_mutex_t notify_lk = PTHREAD_MUTEX_INITIALIZER;

struct notify_args
{
  int len;
  unsigned char data[1];
};

static void run_notify(void *arg)
{
  struct notify_args *args = (struct notify_args *) arg;
  int i;
  for( i=0 ; i<REPEAT ; i++ )
  {
    LOCK(notify_lk);
    broadcast_local(sock, args->data, args->len);
    UNLOCK(notify_lk);
    if( i==REPEAT-1 )
      break;
    else
      sleep(1);
  }
  free(arg);
}

/* notify all clients that the tag
 * is replicated. */
static void notify_interp_rep(struct tag *t)
{
  struct pack_ucmd *ucmd;
  struct notify_args *args;
  int len;

  /* don't use the tag's cast sock.
   * we are not in the casting thread.. */
  if( !sock )
    sock = open_broadcast_sock();

  /* TODO: this is duplicate with brcmd.. */
  len = sizeof(struct pack_ucmd);
  args = (struct notify_args *) malloc(sizeof(struct notify_args)+len);

  ucmd = (struct pack_ucmd *) &args->data[0];
  ucmd->type = PACKET_UCMD;
  ucmd->cmd = UCMD_INTERP_REP_TAG;
  ucmd->u.interp.tag = t->id;
  ucmd->u.interp.rep = t->interp.rep? t->interp.rep->id:0;
  args->len = len;

  async_run(run_notify, args);
}

void interp_set_rep_tag(struct tag *t, struct tag *rep)
{
  t->interp.rep = rep;
  notify_interp_rep(t);
}

void interp_del_rep(struct tag *t)
{
  t->interp.rep = NULL;
  notify_interp_rep(t);
}

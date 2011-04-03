#include "tag.h"
#include "cast.h"
#include <stdio.h>
#include <string.h>
#include "packet.h"
#include "pcm.h"
#include "include/pack.h"
#include "include/debug.h"

struct tag *tag_create(long gid, long tid)
{
  struct tag *t;
  pthread_t thread;
  long long tuid;

  tuid = TAGUID(gid, tid);

  t = (struct tag *)malloc(sizeof (struct tag));
  memset(t, 0, sizeof (struct tag));
  t->tid = tid;
  t->id = tuid;
  //t->name[0]=0;

  INIT_LIST_HEAD(&t->device_head);
  INIT_LIST_HEAD(&t->subscribe_head);

  //memset(t->mix_devs, 0, sizeof t->mix_devs);
  //t->mix_count = 0;

  pthread_mutex_init(&t->mut, NULL);
  pthread_cond_init(&t->cnd_nonempty, NULL);

  //t->bcast_size = 0;

  add_tag(t);

  /* create the tag's casting queue thread */
  pthread_create(&thread, NULL, tag_run_casting, t);

  printf("tag %ld:%ld created\n", gid, tid);
  return t;
}

static int sincmp(const void *s1, const void *s2)
{
  struct sockaddr_in **ps1 = (struct sockaddr_in **) s1;
  struct sockaddr_in **ps2 = (struct sockaddr_in **) s2;
  return (*ps1)->sin_addr.s_addr - (*ps2)->sin_addr.s_addr;
}

void tag_add_bcast(struct tag *t, struct sockaddr_in *bcast)
{
  void *p;

  if (t->bcast_size >= sizeof(t->bcasts)/sizeof(t->bcasts[0]))
  {
    fprintf(stderr, "warning: tag %d exceeding max broadcast addresses!\n", (int)t->tid);
    return;
  }

  if (p = bsearch(&bcast, t->bcasts, t->bcast_size, sizeof(t->bcasts[0]), sincmp))
    /* the address is already in the list */
    return;

  /* add address into the list and sort */
  t->bcasts[t->bcast_size] = bcast;
  qsort(t->bcasts, t->bcast_size+1, sizeof(t->bcasts[0]), sincmp);
  ++t->bcast_size;
}


static void _dev_in_packet(struct device *d, struct packet *p)
{
  struct packet **pp = (struct packet **)cfifo_get_in(&d->pack_fifo);
  *pp = p;
  cfifo_in_signal(&d->pack_fifo);
  trace("avail %d packs in fifo %d\n", cfifo_len(&d->pack_fifo), (int)d->id);
}

/* this must be used when cfifo_empty is false */
static inline struct packet *__dev_out_packet(struct device *d)
{
  struct packet *p;
  p = *(struct packet **)cfifo_get_out(&d->pack_fifo);
  cfifo__out(&d->pack_fifo);
  return p;
}

static struct packet *_dev_out_packet(struct device *d)
{
  cfifo_wait_empty(&d->pack_fifo);
  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this is important as the wait could be canceled
       by the cmd thread */
    return NULL;
  }
  return __dev_out_packet(d);
}


void tag_add_outstanding(struct tag *t, struct device *d)
{
  int i;
  trace("adding dev to outstanding\n");
  /* find a place in the mix_devs,
   * we do not put all the non-empty fifos together at top
   * because we found it too tricky to sync them. */
  for( i=0 ; i<8 ; i++ )
  {
    if( !t->mix_devs[i] )
    {
      t->mix_devs[i] = d;

      d->mixbit = 1<<i;
      t->mix_mask |= d->mixbit;

      d->timeouts = 0;

      pthread_mutex_lock(&t->mut);
      t->mix_count ++;
      pthread_mutex_unlock(&t->mut);
      pthread_cond_signal(&t->cnd_nonempty);

      /* clear the dev's fifo to avoid early lag. */
      cfifo_clear(&d->pack_fifo);

      break;
    }
  }
}

void tag_rm_outstanding(struct tag *t, struct device *d)
{
  int i;
  /* find the device in the mix_devs */
  for( i=0 ; i<8 ; i++ )
  {
    if( d == t->mix_devs[i] )
    {
      trace("removing dev from outstanding\n");
      t->mix_devs[i] = NULL;

      t->mix_mask &= ~d->mixbit;

      pthread_mutex_lock(&t->mut);
      t->mix_count --;
      pthread_mutex_unlock(&t->mut);

      break;
    }
  }

  /* avoid dead lock */
  cfifo_cancel_wait(&d->pack_fifo);

  /* todo: clear the fifo of the dev */
}

void tag_clear_outstanding(struct tag *t)
{
  int i;
  struct device *d;

  if( t->mix_count == 0 )
  {
    return;
  }

  for( i=0 ; i<8 ; i++ )
  {
    if( d = t->mix_devs[i] )
    {
      t->mix_devs[i] = NULL;

      /* avoid dead lock */
      cfifo_cancel_wait(&d->pack_fifo);
    }
  }

  t->mix_mask = 0;

  pthread_mutex_lock(&t->mut);
  t->mix_count = 0;
  pthread_mutex_unlock(&t->mut);

}


void tag_in_dev_packet(struct tag *t, struct device *d, struct packet *pack)
{
  /* put in */
  _dev_in_packet(d, pack);

  /* decide state change */
  t->mix_stat |= d->mixbit;
}

/* this should only be called when dev's fifo is not empty */
struct packet *tag_out_dev_packet(struct tag *t, struct device *d)
{
  struct packet *p;

  /* get out */
  p = __dev_out_packet(d);

  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this fifo has become empty */
    t->mix_stat &= ~d->mixbit;
  }

  return p;
}


static struct packet *tag_mix_audio(struct tag *t);

struct packet *tag_out_dev_mixed(struct tag *t)
{
  struct packet *pack;

  pthread_mutex_lock(&t->mut);
  while( !( t->mix_count > 0 ) )
  {
    /* this is actually not that useful...
     * it's just kept as it's not harmful */
    pthread_cond_wait(&t->cnd_nonempty, &t->mut);
  }
  pthread_mutex_unlock(&t->mut);

  pack = tag_mix_audio(t);

  return pack;
}

static void tag_update_dev_timeouts(struct tag *t)
{
  int i;
  struct device *d;

  for( i=0 ; i<8 ; i++ )
  {
    if( d = t->mix_devs[i] )
    {
      if( cfifo_empty(&d->pack_fifo) )
      {
        if( ++ d->timeouts > 500 )
        {
          /* what? this one has been hung for too long...
           * get this shit out of our way. */
          fprintf(stderr, "%d seems to be hung, clear it.\n",
              (int)d->id);
          tag_rm_outstanding(t, d);
        }
      }
      else
      {
        d->timeouts = 0;
      }
    }
  }
}

#define min(a,b) ((a)<(b)?(a):(b))

static struct packet *tag_mix_audio(struct tag *t)
{
  struct device *d;
  struct packet *pp[8], *p;
  pack_data *aupack;
  short *au[8];
  int mixlen = 1<<18;/* initially a big number is ok */
  int waited = 0;
  int i,c,l;

  /* all outstanding devs must have data on their queue.
   * otherwise clients sending at lower rate may be
   * scattered. */
  trace("stat %02x, mask %02x\n", t->mix_stat, t->mix_mask);
  while( (t->mix_stat & t->mix_mask) != t->mix_mask )
  {
    /* wait for 10ms to allow the data to be queued.
     * this virtually syncs the clients sending
     * at different rates. */
    usleep(10*1000);

    /* find out who are empty. this should be costless
     * compared to our wait. */
    tag_update_dev_timeouts(t);

    if( ++waited > 50 )
    {
      /* it has been quite a while that there's no
       * any data. probably a client has stopped. */
      trace("no data within timeout. maybe someone stopped.\n");
      return NULL;
    }
  }

#define add_mix_pack(p) \
  { \
    pp[c] = p; \
    aupack = (pack_data *) (p)->data; \
    au[c] = (short *) aupack->data; \
    l = ntohl(aupack->datalen); \
    mixlen = min(mixlen, l); \
    c++; \
  }

  /* first pass: flush the over-loaded queues */
  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    if( !d->flushing )
    {
      if( cfifo_len(&d->pack_fifo) > 16 )
      {
        /* trigger flush this queue */
        d->flushing = 1;
        d->stats.flushed ++;
      }
      else
        continue;
    }
    else
    {
      if( cfifo_len(&d->pack_fifo) < 8 )
      {
        /* stop flush this queue */
        d->flushing = 0;
        continue;
      }
    }
    printf("flush queue %ld, %d\n", d->id, d->stats.flushed);

    p = tag_out_dev_packet(t, d);

    add_mix_pack(p);
  }

  if( c > 0 )
    goto mix;

  /* second pass: normal blending */
  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    /* do not wait, just pick the data or go on.
     * we do not wait because we can't rely on the
     * client. consider that if a client claimed it's
     * sending data, but then it crashed. waiting for
     * it's data here will lead to endless wait! */

    /* recheck to ensure the fifo is not empty */
    if( cfifo_empty(&d->pack_fifo) )
      continue;

    /* reset this one's timeouts. we can immediately
     * take data from it's fifo without wait. */
    d->timeouts = 0;

    p = tag_out_dev_packet(t, d);

    add_mix_pack(p);
  }

mix:
  switch (c)
  {
   case 0:
    return NULL;

   case 1:
    /* no need to mix */
    trace("only 1 pack from %d, mix not needed.\n",
          (int)pp[0]->dev->id);
    break;

   default:
   {
     pcm_mix(au, c, mixlen>>1);

     for( i=1 ; i<c ; i++ )
       pack_free(pp[i]);
   }
   break;
  }

  return pp[0];
}

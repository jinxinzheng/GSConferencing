#include "tag.h"
#include "cast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "packet.h"
#include "pcm.h"
#include "include/pack.h"
#include "include/debug.h"
#include "include/types.h"
#include "sys.h"
#include "db/md.h"
#include "opts.h"
#include <include/thread.h>
#include <include/lock.h>
#include <include/util.h>

//#define MIX_DEBUG

struct tag *tag_create(long gid, long tid)
{
  struct tag *t;
  long tuid;
  int i;

  tuid = TAGUID(gid, tid);

  t = (struct tag *)malloc(sizeof (struct tag));
  memset(t, 0, sizeof (struct tag));
  t->id = tid;
  t->tuid = tuid;
  //t->name[0]=0;

  INIT_LIST_HEAD(&t->device_head);

  for( i=0 ; i<MAX_SUB ; i++ )
  {
    INIT_LIST_HEAD(&t->subscribe_head[i]);
  }

  //memset(t->mix_devs, 0, sizeof t->mix_devs);
  //t->mix_count = 0;

  pthread_mutex_init(&t->mut, NULL);
  pthread_cond_init(&t->cnd_nonempty, NULL);

  cfifo_init(&t->cast.rep_reqs, 4, 6);

  INIT_LIST_HEAD(&t->discuss.open_list);
  t->discuss.openuser = 0;

  if( tid == 1 )
  {
    struct group *g = get_group(gid);
    t->discuss.mode = g->db_data->discuss_mode;
    if( g->db_data->discuss_maxuser == 0 )
      t->discuss.maxuser = MAX_MIX;
    else
      t->discuss.maxuser = g->db_data->discuss_maxuser;
  }
  else
  {
    t->discuss.mode = DISCMODE_FIFO;
    t->discuss.maxuser = 1;
  }

  t->interp.mode = INTERP_NO;
  INIT_LIST_HEAD(&t->interp.dup_head);
  pthread_mutex_init(&t->interp.mx, NULL);

  //t->bcast_size = 0;

  add_tag(t);

  /* create the tag's casting queue thread */
  start_thread(tag_run_casting, t);

  trace_info("tag %ld:%ld created\n", gid, tid);
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
    trace_err("tag %d exceeding max broadcast addresses!\n", (int)t->id);
    return;
  }

  if( (p = bsearch(&bcast, t->bcasts, t->bcast_size, sizeof(t->bcasts[0]), sincmp)) )
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
  cfifo__in(&d->pack_fifo);
  trace_dbg("avail %d packs in queue %d\n", cfifo_len(&d->pack_fifo), (int)d->id);
}

/* this must be used when pack queue is not empty */
static inline struct packet *__dev_out_packet(struct device *d)
{
  struct packet *p;
  p = *(struct packet **)cfifo_get_out(&d->pack_fifo);
  cfifo__out(&d->pack_fifo);
  return p;
}


void tag_add_outstanding(struct tag *t, struct device *d)
{
  int i;
  trace_dbg("adding dev to outstanding\n");

  LOCK(t->mut);

  /* find a place in the mix_devs,
   * we do not put all the non-empty queues together at top
   * because we found it too tricky to sync them. */
  for( i=0 ; i<8 ; i++ )
  {
    if( !t->mix_devs[i] )
    {
      t->mix_devs[i] = d;

      d->mixbit = 1<<i;

      d->timeouts = 0;

      /* set the mix reference if not set. */
      if( !t->mix_devs[t->mix_ref] )
      {
        t->mix_ref = i;
      }

      t->mix_count ++;
      pthread_cond_signal(&t->cnd_nonempty);

      break;
    }
  }

  UNLOCK(t->mut);
}

void tag_rm_outstanding(struct tag *t, struct device *d)
{
  int i;

  LOCK(t->mut);

  /* find the device in the mix_devs */
  for( i=0 ; i<8 ; i++ )
  {
    if( d == t->mix_devs[i] )
    {
      trace_dbg("removing dev from outstanding\n");

      /* update the mix reference if it's
       * being removed */
      if( t->mix_ref == i )
      {
        int j;
        for( j=0 ; j<8 ; j++ )
        {
          if( j==i )
            continue;
          if( t->mix_devs[j] )
          {
            t->mix_ref = j;
            break;
          }
        }
      }

      t->mix_devs[i] = NULL;

      d->mixbit = 0;

      t->mix_count --;

      break;
    }
  }

  UNLOCK(t->mut);

  /* todo: clear the queue of the dev */
}


#define MAX_DEV_PACKS 32

void tag_in_dev_packet(struct tag *t, struct device *d, struct packet *pack)
{
  /* don't en-queue if the dev is not outstanding.
   * in the small window when client is opening or
   * closing the mic. */
  if( !d->mixbit )
  {
    pack_free(pack);
    return;
  }

  /* immediately free the packet if the queue
   * is crazily high-loaded, in order to avoid
   * resource exhausting. this usually means
   * we got a broken client ... */
  if( cfifo_full(&d->pack_fifo) )
  {
    pack_free(pack);
    return;
  }

  /* put in */
  _dev_in_packet(d, pack);

}

/* this should only be called when dev's queue is not empty */
struct packet *tag_out_dev_packet(struct tag *t, struct device *d)
{
  struct packet *p;

  /* get out */
  p = __dev_out_packet(d);

  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this queue has become empty */
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
    if( (d = t->mix_devs[i]) )
    {
      if( cfifo_empty(&d->pack_fifo) )
      {
        if( ++ d->timeouts > 300 )
        {
          /* what? this one has been hung for too long...
           * get this shit out of our way. */
          trace_err("%d seems to be hung, clear it.\n",
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

static inline void drop_queue_front(struct device *d)
{
  struct packet *p;

  p = __dev_out_packet(d);

  pack_free(p);
}

static void flush_queue(struct tag *t, struct device *d)
{
  int l;

  trace_warn("flush queue %ld, len %d, %d\n",
      d->id, cfifo_len(&d->pack_fifo), d->stats.flushed);

  l = cfifo_len(&d->pack_fifo);
  for( ; l>1 ; l-- )
  {
    drop_queue_front(d);
  }

  d->stats.flushed ++;
}

static inline void flush_all_queues(struct tag *t)
{
  struct device *d;
  int i;

  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    flush_queue(t, d);
  }
}

static void flush_queue_silence(struct tag *t, struct device *d)
{
  struct packet *p;
  pack_data *pd;
  int l;
  int n=0;

  l = cfifo_len(&d->pack_fifo);

  for( ; l>1 ; l-- )
  {
    /* get packet at front but not getting out */
    p = *(struct packet **)cfifo_get_out(&d->pack_fifo);

    pd = (pack_data *) p->data;
    if( pd->type == PACKET_AUDIO_ZERO )
    {
      drop_queue_front(d);
      n++;
    }
    else
      break;
  }

  if( n>0 )
  {
    d->stats.flushed ++;
    trace_warn("%ld drop silent packs %d\n", d->id, n);
  }
}

static inline int flush_queues(struct tag *t)
{
  struct device *d;
  int i,c,len;

  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    len = cfifo_len(&d->pack_fifo);
    if( len > 6 )
    {
      /* this queue is over-loaded.
       * flush it immediately */
      flush_queue_silence(t, d);
    }
    else if( len > 1 )
    {
      /* this queue is high-loaded. */
      if( ++d->highload > 180 )
      {
        /* it's been high-loaded for about 1 second.
         * trigger flush now. */
        flush_queue_silence(t, d);
      }
      else
        continue;
    }
    else
    {
      /* <2 is considered safe. */
      d->highload = 0;
      continue;
    }

    c++;
  }

  return (c > 0);
}

static inline int queue_bell(struct device *d)
{
  return ! cfifo_empty(&d->pack_fifo);
}

static int all_queues_bell(struct tag *t)
{
  struct device *d;
  int i;

  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    if( !queue_bell(d) )
    {
      return 0;
    }
  }

  return 1;
}

static inline int wait_all_queues(struct tag *t)
{
  int waited = 0;

  while( !(all_queues_bell(t)) )
  {
    trace_dbg("no data, waited=%d\n", waited);
    /* wait for 3ms to allow the data to be queued.
     * this virtually syncs the clients sending
     * at different rates. */
    usleep(3*1000);

    /* find out who are empty. this should be costless
     * compared to our wait. */
    tag_update_dev_timeouts(t);

    if( ++waited > 50 )
    {
      /* it has been quite a while that there's no
       * any data. probably a client has stopped. */
      trace_warn("no data within timeout. maybe someone stopped.\n");
      return 0;
    }
  }

  return 1;
}

static int wait_any_queue(struct tag *t)
{
  int usecs = 11610;  /* for 11k Hz */

  while( usecs > 0 )
  {
    /* immediately return if all queues have data.
     * checked every 1 ms. this ensures us not drifted
     * too much from the realtime data. */
    if( all_queues_bell(t) )
      return 1;

    if( usecs >= 1000 )
    {
      usleep(1000);
      usecs -= 1000;
    }
    else
    {
      usleep(usecs);
      usecs = 0;
    }
  }

  return 1;
}

static int wait_ref_queue(struct tag *t)
{
  struct device *d;

  if( !(d=t->mix_devs[t->mix_ref]) )
  {
    return 0;
  }

  while( !queue_bell(d) )
  {
    if( !d->mixbit )
      /* dev removed in the while? */
      return 0;

    msleep(3);

    if( ++ d->timeouts > 300 )
    {
      trace_err("clear hung %d\n", (int)d->id);
      tag_rm_outstanding(t, d);

      /* good point to sync all queues */
      flush_all_queues(t);
      return 0;
    }
  }

  d->timeouts = 0;

  return 1;
}

static struct packet *tag_mix_audio(struct tag *t)
{
  struct device *d;
  struct packet *pp[8], *p;
  pack_data *aupack;
  short *au[8];
  int mixlen = 1<<18;/* initially a big number is ok */
  int i,c,l;

#ifdef MIX_DEBUG
  static int count = 0;
  count = (count+1) & ((1<<8)-1);
#endif

#define add_mix_pack(p) \
  do { \
    pp[c] = p; \
    aupack = (pack_data *) (p)->data; \
    au[c] = (short *) aupack->data; \
    l = ntohs(aupack->datalen); \
    /* sanity check */  \
    if( l>MAXPACK-100 ) { \
      trace_warn("pack data len=%d, drop!\n", l); \
      pack_free(p); \
      break;  \
    } \
    if( aupack->type == PACKET_AUDIO_ZERO ) {\
      memset(aupack->data, 0, l); \
      aupack->type = PACKET_AUDIO; \
      p->len = offsetof(pack_data, data) + l;  \
    } \
    mixlen = min(mixlen, l); \
    c++; \
  } while(0)

  if( t->mix_count == 1 )
  {
    /* flushing is not needed when there's only 1 queue. */
    goto normal;
  }

  /* first pass: flush the over-loaded queues */

  if( opt_flush )
  {
    flush_queues(t);
  }

normal:
  /* second pass: normal blending */

  switch ( opt_sync_policy )
  {
    case SYNC_WAIT :
    /* wait for all outstanding devs to have data on
     * their queue. otherwise clients sending at
     * lower rate may be scattered. */
    if( !wait_all_queues(t) )
      return NULL;
    break;

    case SYNC_FIXED :
    /* wait for a while hoping to sync all queues at
     * different paces. */
    if( !wait_any_queue(t) )
      return NULL;
    break;

    case SYNC_REF :
    /* wait for one queue selected as 'reference' */
    if( !wait_ref_queue(t) )
      return NULL;
    break;
  }


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

    /* recheck to ensure the queue is not empty */
    if( cfifo_empty(&d->pack_fifo) )
      continue;

#ifdef MIX_DEBUG
    if( !count )
    {
      printf("%ld: len %d\n", d->id, cfifo_len(&d->pack_fifo));
    }
#endif

    /* reset this one's timeouts. we can immediately
     * take data from it's queue without wait. */
    d->timeouts = 0;

    p = tag_out_dev_packet(t, d);

    add_mix_pack(p);
  }

#ifdef MIX_DEBUG
  if( !count )
    printf("\n");
#endif

  switch (c)
  {
   case 0:
    return NULL;

   case 1:
    /* no need to mix */
    break;

   default:
   {
     pcm_mix(au, c, mixlen>>1);

     for( i=1 ; i<c ; i++ )
       pack_free(pp[i]);
   }
   break;
  }

  if( opt_silence_drop )
  {
    /* detect silence packet.
     * dropping continuous silence packets can
     * greatly improve the client's performance.*/
    if( pcm_silent((char *)au[0], mixlen, 51200*c ) )
    {
      if( ++(t->mix_silence) > 50 )
      {
        pack_free(pp[0]);
        trace_dbg("dropping silent pack %d\n", t->mix_silence);
        return NULL;
      }
    }
    else
    {
      t->mix_silence = 0;
    }
  }

  return pp[0];
}

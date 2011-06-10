#include "tag.h"
#include "cast.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "packet.h"
#include "pcm.h"
#include "include/pack.h"
#include "include/debug.h"
#include "include/types.h"
#include "sys.h"
#include "db/md.h"

//#define MIX_DEBUG

struct tag *tag_create(long gid, long tid)
{
  struct tag *t;
  pthread_t thread;
  long long tuid;
  int i;

  tuid = TAGUID(gid, tid);

  t = (struct tag *)malloc(sizeof (struct tag));
  memset(t, 0, sizeof (struct tag));
  t->tid = tid;
  t->id = tuid;
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

  INIT_LIST_HEAD(&t->discuss.open_list);
  t->discuss.maxuser = t->id==1? 1:8; /* todo: move this hard-code to db */
  t->discuss.openuser = 0;

  if( t->id == 1 )
  {
    struct group *g = get_group(gid);
    t->discuss.mode = g->db_data->discuss_mode;
  }
  else
  {
    t->discuss.mode = DISCMODE_FIFO;
  }

  t->interp.mode = INTERP_NO;
  INIT_LIST_HEAD(&t->interp.dup_head);
  pthread_mutex_init(&t->interp.mx, NULL);

  //t->bcast_size = 0;

  add_tag(t);

  /* create the tag's casting queue thread */
  pthread_create(&thread, NULL, tag_run_casting, t);

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
    trace_err("tag %d exceeding max broadcast addresses!\n", (int)t->tid);
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
  cfifo_in_signal(&d->pack_fifo);
  trace_dbg("avail %d packs in fifo %d\n", cfifo_len(&d->pack_fifo), (int)d->id);
}

/* this must be used when cfifo_empty is false */
static inline struct packet *__dev_out_packet(struct device *d)
{
  struct packet *p;
  p = *(struct packet **)cfifo_get_out(&d->pack_fifo);
  cfifo__out(&d->pack_fifo);
  return p;
}

/* not used */
#if 0
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
#endif


void tag_add_outstanding(struct tag *t, struct device *d)
{
  int i;
  trace_dbg("adding dev to outstanding\n");
  /* find a place in the mix_devs,
   * we do not put all the non-empty fifos together at top
   * because we found it too tricky to sync them. */
  for( i=0 ; i<8 ; i++ )
  {
    if( !t->mix_devs[i] )
    {
      /* clear the dev's fifo to avoid early lag. */
      cfifo_clear(&d->pack_fifo);

      t->mix_devs[i] = d;

      d->mixbit = 1<<i;
      t->mix_mask |= d->mixbit;

      d->timeouts = 0;

      pthread_mutex_lock(&t->mut);
      t->mix_count ++;
      pthread_mutex_unlock(&t->mut);
      pthread_cond_signal(&t->cnd_nonempty);

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
      trace_dbg("removing dev from outstanding\n");
      t->mix_devs[i] = NULL;

      t->mix_mask &= ~d->mixbit;
      d->mixbit = 0;

      pthread_mutex_lock(&t->mut);
      t->mix_count --;
      pthread_mutex_unlock(&t->mut);

      break;
    }
  }

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
    if( (d = t->mix_devs[i]) )
    {
      t->mix_devs[i] = NULL;
    }
  }

  t->mix_mask = 0;

  pthread_mutex_lock(&t->mut);
  t->mix_count = 0;
  pthread_mutex_unlock(&t->mut);

}


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
    if( (d = t->mix_devs[i]) )
    {
      if( cfifo_empty(&d->pack_fifo) )
      {
        if( ++ d->timeouts > 500 )
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

static inline int wait_all_queues(struct tag *t)
{
  int waited = 0;

  trace_dbg("stat %02x, mask %02x\n", t->mix_stat, t->mix_mask);
  while( (t->mix_stat & t->mix_mask) != t->mix_mask )
  {
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
  { \
    pp[c] = p; \
    aupack = (pack_data *) (p)->data; \
    au[c] = (short *) aupack->data; \
    l = ntohl(aupack->datalen); \
    mixlen = min(mixlen, l); \
    c++; \
  }

  if( t->mix_count == 1 )
  {
    /* flushing is not needed when there's only 1 queue. */
    goto normal;
  }

  /* first pass: flush the over-loaded queues */
  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    if( !(d = t->mix_devs[i]) )
      continue;

    if( !d->flushing )
    {
      if( cfifo_len(&d->pack_fifo) > 6 )
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
      if( cfifo_len(&d->pack_fifo) < 2 )
      {
        /* stop flush this queue */
        d->flushing = 0;
        continue;
      }
    }

    trace_warn("flush queue %ld, len %d, %d\n",
      d->id, cfifo_len(&d->pack_fifo), d->stats.flushed);

    p = tag_out_dev_packet(t, d);

    pack_free(p);
    c++;
  }

  if( c > 0 )
    /* flushing activated */
    return NULL;

normal:
  /* second pass: normal blending */

  /* when flushing is not activated, must wait for
   * all outstanding devs to have data on their queue.
   * otherwise clients sending at lower rate may be
   * scattered. */
  if( !wait_all_queues(t) )
    return NULL;


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

#ifdef MIX_DEBUG
    if( !count )
    {
      printf("%ld: len %d\n", d->id, cfifo_len(&d->pack_fifo));
    }
#endif

    /* reset this one's timeouts. we can immediately
     * take data from it's fifo without wait. */
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
    trace_dbg("only 1 pack from %d, mix not needed.\n",
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

  return pp[0];
}

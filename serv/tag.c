#include "tag.h"
#include "cast.h"
#include <stdio.h>
#include <string.h>
#include "packet.h"
#include "include/pack.h"
#include "include/debug.h"

struct tag *tag_create(long gid, long tid)
{
  struct tag *t;
  pthread_t thread;
  long long tuid;

  tuid = TAGUID(gid, tid);

  t = (struct tag *)malloc(sizeof (struct tag));
  t->tid = tid;
  t->id = tuid;
  t->name[0]=0;

  INIT_LIST_HEAD(&t->device_head);
  INIT_LIST_HEAD(&t->subscribe_head);

  memset(t->mix_devs, 0, sizeof t->mix_devs);
  t->mix_count = 0;

  pthread_mutex_init(&t->mut, NULL);
  pthread_cond_init(&t->cnd_nonempty, NULL);

  t->bcast_size = 0;

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

static struct packet *_dev_out_packet(struct device *d)
{
  struct packet *p;
  cfifo_wait_empty(&d->pack_fifo);
  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this is important as the wait could be canceled
       by the cmd thread */
    return NULL;
  }
  p = *(struct packet **)cfifo_get_out(&d->pack_fifo);
  cfifo__out(&d->pack_fifo);
  return p;
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
      trace("removing dev from outstanding\n");
      t->mix_devs[i] = NULL;

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

  pthread_mutex_lock(&t->mut);
  t->mix_count = 0;
  pthread_mutex_unlock(&t->mut);

}


void tag_in_dev_packet(struct tag *t, struct device *d, struct packet *pack)
{
  int add = 0;

  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this fifo will become non-empty */
    add=1;
  }

  /* put in */
  _dev_in_packet(d, pack);

  /* decide state change */
}

struct packet *tag_out_dev_packet(struct tag *t, struct device *d)
{
  struct packet *p;
  int rm = 0;

  /* get out */
  p = _dev_out_packet(d);

  if( cfifo_empty(&d->pack_fifo) )
  {
    /* this fifo has become empty */
    rm=1;
  }

  /* decide state change */

  return p;
}


static struct packet *tag_mix_audio(struct tag *t);

struct packet *tag_out_dev_mixed(struct tag *t)
{
  struct packet *pack;

  pthread_mutex_lock(&t->mut);
  while( !( t->mix_count > 0 ) )
  {
    pthread_cond_wait(&t->cnd_nonempty, &t->mut);
  }
  pthread_mutex_unlock(&t->mut);

  pack = tag_mix_audio(t);

  return pack;
}

#define min(a,b) ((a)<(b)?(a):(b))

static struct packet *tag_mix_audio(struct tag *t)
{
  struct device *d;
  struct packet *pp[8], *p;
  pack_data *aupack;
  short *au[8];
  int mixlen = 1<<18;/* initially a big number is ok */
  int i,c,l;

  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    d = t->mix_devs[i];

    if( !d )
      continue;

    if( !(p = _dev_out_packet(d)) )
      continue;

    pp[c] = p;
    aupack = (pack_data *) p->data;
    au[c] = (short *) aupack->data;
    l = ntohl(aupack->datalen);
    mixlen = min(mixlen, l);
    c++;
  }

  if( c == 0 )
  {
    return NULL;
  }
  else if( c == 1 )
  {
    /* no need to mix */
    trace("[%s] only 1 outstanding dev, mix not needed.\n", __func__);
  }
  else
  {
    /* audio params */
    const int bytes=2;
    const int aulen = mixlen/bytes;

    const int zero = 1 << ((bytes*8)-1) ;
    register int mix;

    short **pau;
    short *au0 = au[0];

    for( i=0 ; i<aulen ; i++ )
    {
      mix = 0;
      for( pau=&au[0] ; pau<&au[c] ; pau++ )
      {
        mix += *( (*pau)++ );
      }
      mix /= c;

      if(mix > zero-1) mix = zero-1;
      else if(mix < -zero) mix = -zero;

      au0[i] = (short)mix;
    }

    for( i=1 ; i<c ; i++ )
    {
      pack_free(pp[i]);
    }
  }

  return pp[0];
}

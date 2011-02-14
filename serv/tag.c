#include "tag.h"
#include "cast.h"
#include <stdio.h>
#include <string.h>
#include "packet.h"
#include "include/pack.h"

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

  cfifo_init(&t->pack_fifo, 8, 2); //256 elements of 4 bytes for each
  cfifo_enable_locking(&t->pack_fifo);

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

void tag_enque_packet(struct tag *t, struct packet *p)
{
  struct packet **pp = (struct packet **)cfifo_get_in(&t->pack_fifo);
  *pp = p;
  cfifo_in_signal(&t->pack_fifo);
}

struct packet *tag_deque_packet(struct tag *t)
{
  struct packet *p;
  cfifo_wait_empty(&t->pack_fifo);
  p = *(struct packet **)cfifo_get_out(&t->pack_fifo);
  cfifo__out(&t->pack_fifo);
  return p;
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
  cfifo__in(&d->pack_fifo);
}

/* only call it when the fifo of the dev is not empty */
static struct packet *_dev_out_packet(struct device *d)
{
  struct packet *p;
  p = *(struct packet **)cfifo_get_out(&d->pack_fifo);
  cfifo__out(&d->pack_fifo);
  return p;
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
  if( add )
  {
    int i;
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
}

struct packet *tag_out_dev_packet(struct tag *t, struct device *d)
{
  struct packet *p;
  int rm = 0;

  if( !cfifo_empty(&d->pack_fifo) )
  {
    /* this fifo will become empty */
    rm=1;
  }

  /* put in */
  p = _dev_out_packet(d);

  /* decide state change */
  if( rm )
  {
    int i;
    /* find the device in the mix_devs */
    for( i=0 ; i<8 ; i++ )
    {
      if( d == t->mix_devs[i] )
      {
        t->mix_devs[i] = NULL;

        pthread_mutex_lock(&t->mut);
        t->mix_count --;
        pthread_mutex_unlock(&t->mut);

        break;
      }
    }
  }
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

static struct packet *tag_mix_audio(struct tag *t)
{
  struct device *d;
  struct packet *pp[8];
  short *au[8];
  int i,c;

  c = 0;
  for( i=0 ; i<8 ; i++ )
  {
    d = t->mix_devs[i];

    if( cfifo_empty(&d->pack_fifo) )
      continue;

    pp[c] = tag_out_dev_packet(t, d);
    au[c] = (short *)( (pack_data *) pp[c]->data )->data;
    c++;
  }

  if( c == 1 )
  {
    /* no need to mix */
  }
  else
  {
    /* audio params */
    const int bytes=2;
    const int aulen = 512/bytes;

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

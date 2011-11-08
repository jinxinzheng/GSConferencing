#include <sys/types.h>
#include "sys.h"
#include "include/queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "opts.h"
#include "packet.h"
#include "network.h"
#include "devctl.h"
#include "include/pack.h"
#include "include/types.h"
#include "include/debug.h"
#include <include/lock.h>
#include <include/thread.h>
#include <include/util.h>
#include "db/md.h"
#include "interp.h"

//#define DGB_REPEAT(a...)  fprintf(stderr, "repeat: " a)
#define DGB_REPEAT(a...)

/* packets sent by devices with the same tag will be queued together.
 * There will be as many queues as the number of existing tags.
 * The queued packets will be sent to the 'subscribers' of the tag
 * in sequence.
 * Each queue will be managed within one thread for performance
 * consideration. */
int dev_cast_packet(struct device *dev, int packet_type, struct packet *pack)
{
  pack_data *p = (pack_data *)pack->data;
  struct tag *t;

  trace_verb("%d.%d ", ntohl(p->id), ntohl(p->seq));
  DEBUG_TIME_NOW();

  t = dev->tag;

  /* fill the tag id */
  p->tag = htons((uint16_t)t->id);

  /* enque packet to device's fifo */
  tag_in_dev_packet(t, dev, pack);

  /* dup packet for interpreting */
  if( !list_empty(&t->interp.dup_head) )
  {
    interp_dup_pack(t, pack);
  }

  return 0;
}

static void __repeat_cast(struct tag *t, struct device *d, uint32_t seq)
{
  struct packet *r;
  pack_data *p;
  int pos;
  int i;

  pos = t->cast.rep_pos;
  for( i = REP_CAST_SIZE-1 ; i>=0 ; i-- )
  {
    if( (r=t->cast.rep_pack[(pos+i) & REP_CAST_MASK]) )
    {
      p = (pack_data *)r->data;
      if( ntohl(p->seq) == seq )
      {
        DGB_REPEAT("repeat %d\n", seq);
        /* uni cast to the requester */
        //sendto_dev_udp(t->sock, r->data, r->len, d);

        if( r->rep_count < 8 )
        {
          broadcast(t, r->data, r->len);
          ++ r->rep_count;
        }
        else
          DGB_REPEAT("not repeating %d, rep_count=%d\n", seq, r->rep_count);
        break;
      }
    }
  }

  if( i<0 )
  {
    /* notify all that this pack is outdated,
     * to keep client from unnecessary wait. */
    pack_data resp;
    int len;
    DGB_REPEAT("can't fulfill repeat req for %d\n", seq);

    resp.id = 0;
    resp.seq = seq;
    resp.type = PACKET_SEQ_OUTDATE;
    resp.tag = t->id;
    resp.datalen = 0;

    len = offsetof(pack_data, data);

    P_HTON(&resp);

    broadcast(t, &resp, len);
  }
}

static void __outdate_pack(struct tag *t, struct packet *pack)
{
  struct packet *out;

  out = t->cast.rep_pack[t->cast.rep_pos];

  pack->rep_count = 0;
  t->cast.rep_pack[t->cast.rep_pos] = pack;

  t->cast.rep_pos = (t->cast.rep_pos+1) & REP_CAST_MASK;

  if( out )
    pack_free(out);
}

#if 0
void tag_repeat_cast(struct tag *t, uint32_t seq)
{
  LOCK(t->cast.lk);

  __repeat_cast(t, seq);

  UNLOCK(t->cast.lk);
}

static void *run_outdated(void *arg)
{
  struct tag *t = (struct tag *)arg;
  struct packet *pack;
  struct list_head *e;

  while(1)
  {
    e = blocking_deque(&t->cast.outdate_queue);
    pack = list_entry(e, struct packet, queue_l);

    /* free the most outdated pack, and put one in.
     * this must be mutual-exclusively locked with
     * the repeat cast. */

    LOCK(t->cast.lk);

    __outdate_pack(t, pack);

    UNLOCK(t->cast.lk);
  }

  return NULL;
}

static void tag_outdate_pack(struct tag *t, struct packet *pack)
{
  /* do this in another thread as
   * the locking could be too lengthy. */
  if( !t->cast.outdated )
  {
    blocking_queue_init(&t->cast.outdate_queue);
    t->cast.outdated = start_thread(run_outdated, t);
  }

  /* the queue_l list member is not in use right now. */
  blocking_enque(&t->cast.outdate_queue, &pack->queue_l);
}
#endif

typedef struct
{
  struct device *d;
  uint32_t seq;
} rep_req_t;

void tag_req_repeat(struct tag *t, struct device *d, uint32_t seq)
{
  rep_req_t *in;

  if( opt_tcp_audio )
    return;

  if( cfifo_full(&t->cast.rep_reqs) )
    return ;

  in = (rep_req_t *)cfifo_get_in(&t->cast.rep_reqs);
  in->d = d;
  in->seq = seq;
  cfifo_in(&t->cast.rep_reqs);
}

static void tag_repeats(struct tag *t)
{
  rep_req_t *out;

  if( opt_tcp_audio )
    return;

  while( !cfifo_empty(&t->cast.rep_reqs) )
  {
    out = (rep_req_t *)cfifo_get_out(&t->cast.rep_reqs);

    __repeat_cast(t, out->d, out->seq);

    cfifo_out(&t->cast.rep_reqs);
  }
}

/* send a packet to all of the tag members */
void tag_cast_pack(struct tag *t, struct packet *pack)
{
  struct device *d;
  struct list_head *h;
  int i;

#ifdef DEBUG_VERB
  {
    pack_data *pd = (pack_data *)pack->data;
    trace_verb("%d.%d ", ntohl(pd->id), ntohl(pd->seq));
    DEBUG_TIME_NOW();
  }
#endif

  if (opt_broadcast)
  {
    /* do broadcast here */
    broadcast(t, pack->data, pack->len);
    return;
  }

  for( i=0 ; i<MAX_SUB ; i++ )
  {
    h = &t->subscribe_head[i];

    list_for_each_entry(d, h, subscribe[i])
    {
      /* don't send to device 0.
       * this is a hack... */
      if( d->id == 0 )
        continue;

      if( opt_tcp_audio )
      {
        if( d->audio_sock > 0 )
          dev_send_audio(d, pack->data, pack->len);
      }
      else
      {
        sendto_dev_udp(t->sock, pack->data, pack->len, d);
      }
    }
  }
}

/* threads would be created for each tag
 * and run forever */
void *tag_run_casting(void *tag)
{
  struct tag *t = (struct tag *)tag;
  struct packet *pack;

  /* the udp socket. one created for each thread,
   * so the threads could send data simultaneously
   * without intefering with each other. */
  if ((t->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return NULL;

  if (opt_broadcast)
  {
    int r, optval;
    optval = 1;
    if( (r = setsockopt(t->sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval)) )
    {
      perror("setsockopt(SO_BROADCAST)");
      close(t->sock);
      return NULL;
    }
  }

  do {
    /* fulfill all repeat requests before cast */
    tag_repeats(t);

    /* mix packs from different devices */
    pack = tag_out_dev_mixed(t);
    trace_dbg("got mixed pack=%x from tag\n", (uint32_t)pack);

    /* the pack might be NULL since it could be
     * returned from a canceled wait. */
    if( !pack )
      continue;

    tag_cast_pack(t, pack);

    //((char*)pack->data)[pack->len]=0;
    //printf("cast packet from %d: %s\n", *(int *)pack->data, ((char*)pack->data) +sizeof(int));

    //tag_outdate_pack(t, pack);
    __outdate_pack(t, pack);

  } while (1);

}

/* subscribe packets got from the tag */
int dev_subscribe(struct device *dev, struct tag *tag)
{
  int found = -1;
  int i;
  for( i=0 ; i<MAX_SUB ; i++ )
  {
    if( dev->subscription[i] == tag )
    {
      return 0;
    }
    else
    if( !dev->subscription[i] )
    {
      /* the first empty place */
      if( found < 0)
        found = i;
    }
  }
  if( found < 0 )
  {
    /* replace the first sub if they are full */
    i = 0;
  }
  else
  {
    i = found;
  }

  if( dev->subscription[i] )
    list_move(&dev->subscribe[i], &tag->subscribe_head[i]);
  else
    list_add(&dev->subscribe[i], &tag->subscribe_head[i]);

  dev->subscription[i] = tag;

  trace_info("device %ld subscribed to tag %ld\n", dev->id, tag->tid);

  {
    int *states[] = { &dev->db_data->sub1, &dev->db_data->sub2 };
    *(states[i]) = tag->id;
    device_save(dev);
  }

  /* update the interp dup */
  if( dev->tag->interp.mode == INTERP_RE &&
      dev->tag->interp.curr_dev == dev )
  {
    interp_add_dup_tag(dev->tag, tag);
  }

  return 0;
}

void dev_unsubscribe(struct device *d, struct tag *t)
{
  int i;

  for( i=0 ; i<MAX_SUB ; i++ )
  {
    if( d->subscription[i] == t )
    {
      d->subscription[i] = NULL;
      list_del(&d->subscribe[i]);

      if( 0 == i )
        d->db_data->sub1 = 0;
      else if( 1 == i )
        d->db_data->sub2 = 0;
      device_save(d);

      break;
    }
  }
}

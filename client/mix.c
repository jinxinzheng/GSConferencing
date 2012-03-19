#include  <include/util.h>
#include  <include/pack.h>
#include  <include/cfifo.h>
#include  <string.h>
#include  <stdio.h>
#include  "pcm.h"

#define QUEUE_NUM 8

#define RECENT_SIZE (1<<2)
#define RECENT_MASK (RECENT_SIZE-1)

static struct pack_queue {
  int dev_id;
  uint32_t recent_seqs[RECENT_SIZE];
  int recent_pos;
  int active;
  struct cfifo fifo;
} queues[QUEUE_NUM];
static int dev_count;
static int act_count;

void mix_audio_init()
{
  int i;
  for( i=0 ; i<QUEUE_NUM ; i++ )
  {
    struct pack_queue *q = &queues[i];
    q->dev_id = 0;
    cfifo_init(&q->fifo, 5, 10); /* 32 * 1K */
  }
  dev_count = 0;
  act_count = 0;
}

static int get_queue_index(int dev_id, int new)
{
  int i;
  int avi=-1;
  if( dev_id==0 )
  {
    return -1; //invalid id
  }
  for( i=0 ; i<QUEUE_NUM ; i++ )
  {
    if( queues[i].dev_id == dev_id )
    {
      return i;
    }
    else if( queues[i].dev_id==0 )
    {
      if( avi==-1)
        avi = i;
    }
  }
  if( avi==-1 )
  {
    /* all queues in use */
    return -1;
  }
  if( !new )
  {
    return -1;
  }
  /* set a new queue for it */
  queues[avi].dev_id = dev_id;
  dev_count ++;
  return avi;
}

static int find_queue_index(int dev_id)
{
  return get_queue_index(dev_id, 0);
}

static int create_queue_index(int dev_id)
{
  return get_queue_index(dev_id, 1);
}

static void remove_queue(int index)
{
  struct pack_queue *q = &queues[index];
  cfifo_clear(&q->fifo);
  q->dev_id = 0;
  dev_count --;
  if( q->active )
  {
    q->active = 0;
    act_count --;
  }
}

void mix_audio_open(int dev_id)
{
  /* alloc a queue for the dev */
  int i = create_queue_index(dev_id);
  if( i<0 )
  {
    printf("can't create new queue for %d\n", dev_id);
    //TODO: find an inactive queue to use
    return;
  }
  /* the queue is inactive until first pack arrives */
  queues[i].active = 0;
}

void mix_audio_close(int dev_id)
{
  int i = find_queue_index(dev_id);
  if( i<0 )
  {
    return;
  }
  remove_queue(i);
}

static void flush_queue(struct pack_queue *q)
{
  int l = cfifo_len(&q->fifo);
  printf("flush queue %d, len %d\n", q->dev_id, l);
  for( ; l>1 ; l-- )
  {
    cfifo__out(&q->fifo);
  }
}

static void cleanup_queues()
{
  /* find out timed-out queues and remove them. */
  int i;
  for( i=0 ; i<QUEUE_NUM ; i++ )
  {
    struct pack_queue *q = &queues[i];
    if( q->dev_id==0 )
    {
      continue;
    }
    if( cfifo_empty(&q->fifo) )
    {
      printf("clear hung queue %d\n", q->dev_id);
      remove_queue(i);
      continue;
    }
    cfifo_clear(&q->fifo);
  }
}

static int is_recved(int index, uint32_t seq)
{
  struct pack_queue *q;
  int i;
  q = &queues[index];
  for( i=0 ; i<RECENT_SIZE ; i++ )
  {
    if( seq == q->recent_seqs[(q->recent_pos+RECENT_SIZE-i) & RECENT_MASK] )
    {
      return 1;
    }
  }
  /* add the seq to recent.
   * the list is an 'fifo'. */
  q->recent_pos = (q->recent_pos+1) & RECENT_MASK;
  q->recent_seqs[q->recent_pos] = seq;
  return 0;
}

int put_mix_audio(struct pack *pack)
{
  int i = find_queue_index(pack->id);
  struct pack_queue *q;
  if( i<0 )
  {
    return -1;
  }
  q = &queues[i];
  if( !q->active )
  {
    /* the first pack */
    q->active = 1;
    act_count ++;
  }
  /* remove dup packs */
  if( is_recved(i, pack->seq) )
  {
    return -1;
  }
  if( act_count <= 1 )
  {
    /* optimized for only one queue.
     * mix is not needed. */
    return 0;
  }
  else
  {
    void *in;
    if( cfifo_full(&q->fifo) )
    {
      /* someone unexpectedly stopped? */
      cleanup_queues();
      return -1;
    }
    /* put pack into queue */
    in = cfifo_get_in(&q->fifo);
    memcpy(in, pack, PACK_LEN(pack));
    cfifo_in(&q->fifo);
    return 1;
  }
}

struct pack *get_mix_audio()
{
  short *pcms[QUEUE_NUM];
  int i;
  int samples;
  struct pack *pack0;
  int c;
  if( act_count<2 )
  {
    return NULL;
  }
  /* make sure all queues have data */
  for( i=0 ; i<QUEUE_NUM ; i++ )
  {
    struct pack_queue *q = &queues[i];
    if( q->dev_id!=0 && q->active )
      if( cfifo_empty(&q->fifo) )
        return NULL;
  }
  c = 0;
  pack0 = NULL;
  for( i=0 ; i<QUEUE_NUM ; i++ )
  {
    struct pack_queue *q = &queues[i];
    struct pack *out;
    if( q->dev_id==0 || !q->active )
    {
      continue;
    }
    if( cfifo_len(&q->fifo) > 4 )
    {
      flush_queue(q);
    }
    out = (struct pack *)cfifo_get_out(&q->fifo);
    cfifo__out(&q->fifo);
    if( c==0 )
      pack0 = out;
    pcms[c++] = (short *)out->data;
  }
  if( c==0 )
  {
    return NULL;
  }
  samples = pack0->datalen / 2;
  pcm_mix(pcms, c, samples);
  /* it's safe to return the out'ed
   * data as the mix routines are designed to
   * be synchronized. */
  return pack0;
}

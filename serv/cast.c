#include <sys/types.h>
#include "sys.h"
#include "include/queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opts.h"
#include "packet.h"

static inline void tag_enque_packet(struct tag *t, struct packet *p)
{
  blocking_enque(&t->pack_queue, &p->l);
}

static inline struct packet *tag_deque_packet(struct tag *t)
{
  struct list_head *p;
  p = blocking_deque(&t->pack_queue);
  return list_entry(p, struct packet, l);
}

static inline int tag_queue_len(struct tag *t)
{
  return t->pack_queue.len;
}

/* packets sent by devices with the same tag will be queued together.
 * There will be as many queues as the number of existing tags.
 * The queued packets will be sent to the 'subscribers' of the tag
 * in sequence.
 * Each queue will be managed within one thread for performance
 * consideration. */
int dev_cast_packet(struct device *dev, int packet_type, struct packet *pack)
{
  struct tag *t;

  t = dev->tag;

  if (opt_queue_max!=0 && tag_queue_len(t) >= opt_queue_max)
  {
    /* discard if the queue/buffer is full.
     * this may help to reduce the latency. */
    fprintf(stderr, "buffer %d is full, packet is dropped\n", (int)t->tid);
    return 1;
  }

  /* notify the processing thread to go */
  tag_enque_packet(t, pack);

  return 0;
}

/* send a packet to all of the tag members */
void tag_cast_pack(struct tag *t, struct packet *pack)
{
  struct device *d;
  struct list_head *p, *h;
  h = &t->subscribe_head;

  list_for_each(p, h)
  {
    d = list_entry(p, struct device, subscribe);

    /* don't send back to the original sender.
     * this could help to reduce network load. */
    if (d == pack->dev)
      continue;

    sendto_dev_udp(t->sock, pack->data, pack->len, d);
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

  do {

    /* wait for the main thread data ready */
    pack = tag_deque_packet(t);

    tag_cast_pack(t, pack);

    //((char*)pack->data)[pack->len]=0;
    //printf("cast packet from %d: %s\n", *(int *)pack->data, ((char*)pack->data) +sizeof(int));

    pack_free(pack);

  } while (1);

}

/* subscribe packets got from the tag */
int dev_subscribe(struct device *dev, struct tag *tag)
{
  if (dev->subscription)
    list_move(&dev->subscribe, &tag->subscribe_head);
  else
    list_add(&dev->subscribe, &tag->subscribe_head);

  dev->subscription = tag;

  printf("device %ld subscribed to tag %ld\n", dev->id, tag->tid);

  return 0;
}

void dev_unsubscribe(struct device *dev)
{
  dev->subscription = NULL;
  list_del(&dev->subscribe);
}

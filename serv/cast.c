#include <sys/types.h>
#include "sys.h"
#include "include/queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opts.h"
#include "packet.h"
#include "include/pack.h"
#include "include/debug.h"

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

  trace("[%s] %d.%d ", __func__, ntohl(p->id), ntohl(p->seq));
  DEBUG_TIME_NOW();

  t = dev->tag;

  /* fill the tag id */
  p->tag = htons((uint16_t)t->id);

  /* enque packet to device's fifo */
  tag_in_dev_packet(t, dev, pack);

  return 0;
}

/* send a packet to all of the tag members */
void tag_cast_pack(struct tag *t, struct packet *pack)
{
  struct device *d;
  struct list_head *p, *h;
  h = &t->subscribe_head;

  {
    pack_data *pd = (pack_data *)pack->data;
    trace("[%s] %d.%d ", __func__, ntohl(pd->id), ntohl(pd->seq));
    DEBUG_TIME_NOW();
  }

  if (opt_broadcast)
  {
    /* do broadcast here */
    broadcast(t, pack->data, pack->len);
    return;
  }

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

  if (opt_broadcast)
  {
    int r, optval;
    optval = 1;
    if (r = setsockopt(t->sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval))
    {
      perror("setsockopt(SO_BROADCAST)");
      close(t->sock);
      return NULL;
    }
  }

  do {

    /* mix packs from different devices */
    pack = tag_out_dev_mixed(t);

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

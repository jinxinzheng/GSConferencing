#include "tag.h"
#include "cast.h"
#include <stdio.h>

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
  t->device_list = NULL;
  INIT_LIST_HEAD(&t->subscribe_head);
  cfifo_init(&t->pack_fifo, 8, 2); //256 elements of 4 bytes for each
  cfifo_enable_locking(&t->pack_fifo);
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

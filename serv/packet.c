#include "packet.h"
#include <stdlib.h>

/* packet objects are pooled/recycled
 * to improve performance */
static LIST_HEAD(free_list);

/* lock ensuring thread safety
 * when multi threads access free_list concurrently */
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

struct packet *pack_get_new()
{
  struct packet *p;
  if (list_empty(&free_list))
    p = (struct packet *) malloc(sizeof(struct packet));
  else
  {
    struct list_head *l;

    pthread_mutex_lock(&m);

    l = free_list.next;
    p = list_entry(l, struct packet, free);
    list_del(l);

    pthread_mutex_unlock(&m);
  }

  return p;
}

void pack_free(struct packet *p)
{
  pthread_mutex_lock(&m);

  list_add(&p->free, &free_list);

  pthread_mutex_unlock(&m);
}

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "list.h"
#include <pthread.h>

static inline void enque(struct list_head *head, struct list_head *p)
{
  list_add_tail(p, head);
}

static inline struct list_head *deque(struct list_head *head)
{
  if (list_empty(head))
    return NULL;
  else
  {
    struct list_head *p = head->next;
    list_del(p);
    return p;
  }
}

static inline struct list_head *queue_get_front(struct list_head *head)
{
  if (list_empty(head))
    return NULL;
  else
    return head->next;
}

/* blocking queue implement */

struct blocking_queue
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct list_head head;

  int len;
};

static inline void blocking_queue_init(struct blocking_queue *q)
{
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
  INIT_LIST_HEAD(&q->head);
  q->len = 0;
}

static inline void blocking_enque(struct blocking_queue *q, struct list_head *p)
{
  pthread_mutex_lock(&q->mutex);

  enque(&q->head, p);
  q->len++;

  /* unblock all threads that are waiting to
   * deque or get_front */
  pthread_cond_broadcast(&q->cond);

  pthread_mutex_unlock(&q->mutex);
}

/* this always keeps at least min items in queue */
static inline struct list_head *blocking_deque_min(struct blocking_queue *q, int min)
{
  struct list_head *p;

  pthread_mutex_lock(&q->mutex);

  while (!( q->len > min ))
  {
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  p = deque(&q->head);
  q->len--;

  pthread_mutex_unlock(&q->mutex);

  return p;
}

static inline struct list_head *blocking_deque(struct blocking_queue *q)
{
  return blocking_deque_min(q, 0);
}

static inline struct list_head *blocking_get_front(struct blocking_queue *q)
{
  struct list_head *p;

  pthread_mutex_lock(&q->mutex);

  while (!( p = queue_get_front(&q->head) ))
  {
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  pthread_mutex_unlock(&q->mutex);

  return p;
}

#endif

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

/* blocking queue implement */

struct blocking_queue
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct list_head head;
};

static inline void blocking_queue_init(struct blocking_queue *q)
{
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
  INIT_LIST_HEAD(&q->head);
}

static inline void blocking_enque(struct blocking_queue *q, struct list_head *p)
{
  pthread_mutex_lock(&q->mutex);

  enque(&q->head, p);

  pthread_mutex_unlock(&q->mutex);

  pthread_cond_signal(&q->cond);
}

static inline struct list_head *blocking_deque(struct blocking_queue *q)
{
  struct list_head *p;

  pthread_mutex_lock(&q->mutex);

  while ((p = deque(&q->head)) == NULL)
  {
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  pthread_mutex_unlock(&q->mutex);

  return p;
}

#endif
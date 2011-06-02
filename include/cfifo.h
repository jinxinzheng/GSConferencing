#ifndef _CFIFO_H_
#define _CFIFO_H_

/* circular fifo
 * no need of locking if only one reader
 * and one writer is using it. */

#include <stdlib.h>
#include <pthread.h>

struct cfifo
{
  unsigned int in;
  unsigned int out;
  unsigned int mask;  /* size-1 */
  unsigned int eord;  /* element order */
  void *data;

  /* block the reader if the fifo is empty. */
  pthread_mutex_t empty_mux;
  pthread_cond_t empty_cnd;
};

#define CFIFO(ff, sord, eord) \
  char ff##_data[(1<<(sord)) * (1<<(eord))]; \
  struct cfifo ff = \
  { \
    0, \
    0, \
    (1<<(sord))-1, \
    eord,  \
    ff##_data, \
  }

static inline void cfifo_init(struct cfifo *cf, int size_order, int e_order)
{
  unsigned int size = 1<<size_order;
  unsigned int esize = 1<<e_order;
  cf->mask = size-1;
  cf->eord = e_order;
  cf->data = malloc(size * esize);
  cf->in = 0;
  cf->out = 0;
}

/* use with cfifo_in_signal() in writer and cfifo_wait_empty() in reader. */
static inline void cfifo_enable_locking(struct cfifo *cf)
{
  pthread_mutex_init(&cf->empty_mux, NULL);
  pthread_cond_init(&cf->empty_cnd, NULL);
}

static inline int cfifo_len(struct cfifo *cf)
{
  return (cf->in+(cf->mask+1)-cf->out) & cf->mask;
}

static inline int cfifo_full(struct cfifo *cf)
{
  return (((cf->in+1) & cf->mask) == cf->out);
}

static inline int cfifo_empty(struct cfifo *cf)
{
  return (cf->out == cf->in);
}

static inline void cfifo_clear(struct cfifo *cf)
{
  cf->out = cf->in;
}

/* get the buffer pointed to by in.
 * could avoid memcpy. */
static inline void *cfifo_get_in(struct cfifo *cf)
{
  return ((char *)cf->data) + (cf->in << cf->eord);
}

/* fast. no sanity check. */
static inline void cfifo__in(struct cfifo *cf)
{
  cf->in = (cf->in+1) & cf->mask;
}

static inline int cfifo_in(struct cfifo *cf)
{
  /* no need to check full for that it will get back to
   * empty when full, i.e. circular. */
  //if (cfifo_full(cf))
  //  return 1;

  cfifo__in(cf);

  return 0;
}

static inline int cfifo_in_signal(struct cfifo *cf)
{
  pthread_mutex_lock(&cf->empty_mux);

  cfifo__in(cf);

  pthread_cond_signal(&cf->empty_cnd);

  pthread_mutex_unlock(&cf->empty_mux);

  return 0;
}

/* get the buffer pointed to by out.
 * could avoid memcpy and buffer over run. */
static inline const void *cfifo_get_out(struct cfifo *cf)
{
  return ((const char *)cf->data) + (cf->out << cf->eord);
}

/* fast. no sanity check. */
static inline void cfifo__out(struct cfifo *cf)
{
  cf->out = (cf->out+1) & cf->mask;
}

static inline int cfifo_out(struct cfifo *cf)
{
  if (cfifo_empty(cf))
    return 1;

  cfifo__out(cf);

  return 0;
}

static inline void cfifo_wait_empty(struct cfifo *cf)
{
  pthread_mutex_lock(&cf->empty_mux);
  while( cfifo_empty(cf) )
  {
    pthread_cond_wait(&cf->empty_cnd, &cf->empty_mux);
  }
  pthread_mutex_unlock(&cf->empty_mux);
}

#endif

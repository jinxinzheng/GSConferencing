#include "packet.h"
#include <stdlib.h>
#include "include/cfifo.h"

/* packet objects are pooled/recycled
 * to improve performance */
/* 512 elements, 4 bytes each. */
static CFIFO(free_fifo, 9, 2);

struct packet *pack_get_new()
{
  struct packet *p;
  /* we are never empty */
  if (cfifo_empty(&free_fifo))
    p = (struct packet *) malloc(sizeof(struct packet));
  else
  {
    p = *(struct packet **)cfifo_get_out(&free_fifo);
    cfifo__out(&free_fifo);
  }

  return p;
}

void pack_free(struct packet *p)
{
  struct packet **pp = (struct packet **)cfifo_get_in(&free_fifo);
  *pp = p;
  cfifo__in(&free_fifo);
}

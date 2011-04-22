/*
 * pantilt control code
 * */

#include "dev.h"
#include "sys.h"
#include "db/md.h"
#include <stdio.h>

/* send the cmd to the 'ptc' device,
 * not the device itself. */
static void ptc_send(struct device *d)
{
  struct device *ptc;

  if( ptc = get_device(d->db_data->ptc_id) )
  {
    char *ptcmd = d->db_data->ptc_cmd;
    if( ptcmd[0] )
    {
      char buf[256];
      int l;
      l = sprintf(buf, "%d ptc %s\n", (int)d->id, ptcmd);
      sendto_dev_tcp(buf, l, ptc);
    }
  }
}

static LIST_HEAD(ptc_head);
static struct device *curr = NULL;

void ptc_push(struct device *d)
{
  list_add(&d->ptc.l, &ptc_head);

  /* send ptc cmd for newly pushed dev */
  ptc_send(d);

  curr = d;
}

void ptc_remove(struct device *d)
{
  /* is it pushed in the list? */
  if( !d->ptc.l.prev || !d->ptc.l.next )
  {
    return;
  }

  list_del(&d->ptc.l);

  if( d == curr )
  {
    /* if the lastly pushed dev is removed,
     * the ptc should go to the prev dev. */
    if( !list_empty(&ptc_head) )
    {
      curr = list_entry(ptc_head.next, struct device, ptc.l);
      ptc_send(curr);
    }
    else
    {
      curr = NULL;
    }
  }
}

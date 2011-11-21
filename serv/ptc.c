/*
 * pantilt control code
 * */

#include "dev.h"
#include "sys.h"
#include "db/md.h"
#include <stdio.h>
#include "devctl.h"

static void ptc_send(struct device *d);

/* all the ptc devs that we have ever known. */
static struct device *ptcs[32] = {NULL};

void add_ptc(struct device *ptc)
{
  int i;

  for( i=0 ; i<sizeof(ptcs)/sizeof(ptcs[0]) ; ++i )
  {
    if( ptc == ptcs[i] )
      break;

    if( !ptcs[i] )
    {
      ptcs[i] = ptc;
      break;
    }
  }
}

static void ptc_default()
{
  int i;
  for( i=0 ; i<sizeof(ptcs)/sizeof(ptcs[0]) ; ++i )
  {
    if( !ptcs[i] )
      break;

    ptc_send(ptcs[i]);
  }
}

/* send the cmd to the 'ptc' device,
 * not the device itself. */
static void ptc_send(struct device *d)
{
  struct device *ptc;

  if( (ptc = get_device(d->db_data->ptc_id)) )
  {
    char *ptcmd = d->db_data->ptc_cmd;
    if( ptcmd[0] )
    {
      char buf[256];
      int l;
      l = sprintf(buf, "%d ptc %d %s\n", (int)d->id, (int)ptc->id, ptcmd);
      device_cmd(ptc, buf, l);
    }

    add_ptc(ptc);
  }
}

static LIST_HEAD(ptc_head);
static struct device *curr = NULL;

void ptc_put(struct device *d)
{
  /* is it pushed? */
  if( !d->ptc.l.prev || !d->ptc.l.next )
  {
    /* fresh add. */
    list_add(&d->ptc.l, &ptc_head);
  }
  else
  {
    /* it's already pushed. move to the top. */
    list_move(&d->ptc.l, &ptc_head);
  }

  curr = d;
}

void ptc_push(struct device *d)
{
  ptc_put(d);

  /* send ptc cmd for newly pushed dev */
  ptc_send(d);
}

void ptc_remove(struct device *d)
{
  /* is it pushed in the list? */
  if( !d->ptc.l.prev || !d->ptc.l.next )
  {
    return;
  }

  list_del(&d->ptc.l);

  if( list_empty(&ptc_head) )
  {
    /* if all are removed, the ptc should
     * go to the default position. */
    ptc_default();

    curr = NULL;

    return;
  }

  if( d == curr )
  {
    /* if the lastly pushed dev is removed,
     * the ptc should go to the prev dev. */
    curr = list_entry(ptc_head.next, struct device, ptc.l);
    ptc_send(curr);
  }
}

void ptc_go_current()
{
  if( curr )
  {
    ptc_send(curr);
  }
  else
  {
    ptc_default();
  }
}

int is_ptc(struct device *d)
{
  return (d->db_data->ptc_id == d->id);
}

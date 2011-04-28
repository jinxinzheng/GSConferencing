#include "include/pack.h"
#include "include/types.h"
#include "packet.h"

static void _interp_add_dup_tag(struct tag *t, struct tag *dup)
{
  if( t->interp.dup )
  {
    list_move(&t->interp.dup_l, &dup->interp.dup_head);
  }
  else
  {
    list_add(&t->interp.dup_l, &dup->interp.dup_head);
  }

  t->interp.dup = dup;
}

static void _interp_del_dup_tag(struct tag *t)
{
  list_del(&t->interp.dup_l);
  t->interp.dup = NULL;
}

void interp_add_dup_tag(struct tag *t, struct tag *dup)
{
  pthread_mutex_lock(&dup->interp.mx);
  _interp_add_dup_tag(t, dup);
  pthread_mutex_unlock(&dup->interp.mx);
}

void interp_del_dup_tag(struct tag *t)
{
  struct tag *dup = t->interp.dup;
  pthread_mutex_lock(&dup->interp.mx);
  _interp_del_dup_tag(t);
  pthread_mutex_unlock(&dup->interp.mx);
}

void interp_dup_pack(struct tag *dup, struct packet *pack)
{
  struct tag *t;
  struct list_head *e;
  struct packet *newp;
  pack_data *pdata;

  pthread_mutex_lock(&dup->interp.mx);

  list_for_each(e, &dup->interp.dup_head)
  {
    t = list_entry(e, struct tag, interp.dup_l);

    /* sanity check. */
    if( t->interp.mode == INTERP_NO ||
        t->interp.dup != dup )
    {
      continue;
    }

    /* only dup when the current dev is closed. */
    if( !t->interp.curr_dev ||
        t->interp.curr_dev->discuss.open )
    {
      continue;
    }

    newp = pack_dup(pack);

    pdata = (pack_data *)(newp->data);
    pdata->tag = htons((uint16_t)t->id);

    tag_in_dev_packet(t, t->interp.curr_dev, newp);
  }

  pthread_mutex_unlock(&dup->interp.mx);
}

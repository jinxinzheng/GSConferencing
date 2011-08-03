#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sys.h"
#include "group.h"
#include "db/md.h"
#include "vote.h"
#include "strutil.h"
#include "include/debug.h"

struct group *group_create(long gid)
{
  struct group *g;
  struct db_group *dg;

  if( (dg = md_find_group(gid)) )
  {
    /* any update? */
  }
  else
  {
    /* todo: insert new group in db. */
    return NULL;
  }

  g = (struct group *)malloc(sizeof (struct group));
  memset(g, 0, sizeof(struct group));

  g->id = gid;
  INIT_LIST_HEAD(&g->device_head);
  add_group(g);

  g->db_data = dg;

  /* restore states from db */

  if( dg->discuss_id )
  {
    struct db_discuss *dsc;
    if( (dsc = md_find_discuss(dg->discuss_id)) )
    {
      group_setup_discuss(g, dsc);
    }
  }

  if( dg->regist_start )
  {
    g->regist.expect = dg->regist_expect;
    g->regist.arrive = dg->regist_arrive;
  }

  if( dg->vote_id )
  {
    struct db_vote *dv;
    if( (dv = md_find_vote(dg->vote_id)) )
    {
      group_setup_vote(g, dv);

      g->vote.v = vote_new();
      g->vote.v->cn_options = dv->options_count;
    }
  }

  trace_info("group %ld created\n", gid);
  return g;
}

void group_save(struct group *g)
{
  md_update_group(g->db_data);
}

void append_dev_ents_cache(struct group *g, struct device *d)
{
  LIST_ADD_FMT(g->caches.dev_ents, g->caches.dev_ents_len, "%ld:%d:%s",
      d->id,
      d->type,
      d->db_data->user_id);
}

void refresh_dev_ents_cache(struct group *g)
{
  struct device *d;
  struct list_head *e;

  g->caches.dev_ents[0] = 0;
  g->caches.dev_ents_len = 0;

  list_for_each(e, &g->device_head)
  {
    d = list_entry(e, struct device, list);
    append_dev_ents_cache(g, d);
  }
}

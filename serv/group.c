#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "group.h"
#include "db/md.h"

struct group *group_create(long gid)
{
  struct group *g;
  struct db_group *dg;

  if( dg = md_find_group(gid) )
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

  if( dg->discuss_id )
  {
    g->discuss.current = md_find_discuss(dg->discuss_id);
  }

  printf("group %ld created\n", gid);
  return g;
}

void group_save(struct group *g)
{
  md_update_group(g->db_data);
}

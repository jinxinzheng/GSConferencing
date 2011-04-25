#include "cmd_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "include/types.h"
#include "include/debug.h"
#include "dev.h"
#include "sys.h"
#include "db/md.h"

struct vote *vote_new()
{
  struct vote *v;

  v = (struct vote *)malloc(sizeof (struct vote));
  memset(v, 0, sizeof (struct vote));
  INIT_LIST_HEAD(&v->device_head);

  return v;
}

void vote_results_to_str( char *str, const struct vote *v )
{
  /* results are separated by ',' */

  int i, l=0;

  for( i=0; i<v->cn_options; i++ )
  {
    LIST_ADD_NUM(str, l, v->results[i]);
  }
}

int handle_cmd_votectrl(struct cmd *cmd)
{
  char *scmd, *p;
  char buf[1024];
  int ai=0, i,j,l;

  static struct db_vote *db[1024];
  static int dbl;

  struct device *d;
  struct group *g;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  g = d->group;

  scmd = cmd->args[ai++];
  if (!scmd)
    return 1;

  if (strcmp(scmd, "query") == 0)
  {
    REP_ADD(cmd, "OK");

    /* get a snapshot of the database
     * which is valid until stop. */
    dbl = md_get_array_vote(db);

    l=0;
    for (i=0; i<dbl; i++)
    {
      LIST_ADD(buf, l, db[i]->name);
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);
  }
  else if (strcmp(scmd, "select") == 0)
  {
    p = cmd->args[ai++];
    if (!p)
      return 1;

    i = atoi(p);
    if (i >= dbl) {
      trace_warn("invalid vote select number.\n");
      return 1;
    }

    REP_ADD(cmd, "OK");

    p = db[i]->members;
    REP_ADD(cmd, p);

    REP_END(cmd);
  }
  else if (strcmp(scmd, "start") == 0)
  {
    struct device *m;
    struct vote *v;
    struct db_vote *dv;
    int num;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    num = atoi(p);
    if (num >= dbl) {
      trace_warn("invalid vote start number.\n");
      return 1;
    }

    dv = db[num];

    REP_ADD(cmd, "OK");

    j = dv->type;
    REP_ADD_NUM(cmd, j);

    REP_END(cmd);

    /* create new vote */
    v = vote_new(dv);
    v->cn_options = dv->options_count;
    v->n_members = 0;

    /* set the vote object on the master device
     * but do not add it to the vote list */
    d->vote.v = v;

    /*send vote start cmd to the members*/
    strcpy(buf, dv->members);
    if (strcmp(buf, "all")==0)
    {
      struct list_head *t;

#define device_vote_start() do { \
      sendto_dev_tcp(cmd->rep, cmd->rl, m); \
      vote_add_device(v, m); \
      m->vote.v = v; \
      m->vote.choice = -1; \
      } while (0)

      /*send to all within this group*/
      list_for_each(t, &g->device_head)
      {
        m = list_entry(t, struct device, list);
        device_vote_start();
        ++ v->n_members;
      }
    }
    else
    {
      /*send to only the specified members*/
      IDLIST_FOREACH_p(buf)
      {
        m = get_device(atoi(p));
        if (m) {
          device_vote_start();
        }
        ++ v->n_members;
      }
    }

    d->db_data->vote_master = 1;
    device_save(d);

    g->vote.current = dv;
    g->vote.curr_num = num;

    g->db_data->vote_id = dv->id;
    group_save(g);
  }
  else if (strcmp(scmd, "result") == 0)
  {
    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    /* the vote result */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    i = atoi(p);

    if (!d->vote.v)
      /* bug or corrupt of network packet */
      return 1;

    /* update results */
    d->vote.choice = i;
    d->vote.v->results[i]++;

    REP_OK(cmd);

    d->db_data->vote_choice = i;
    vote_results_to_str( g->db_data->vote_results, d->vote.v );
    device_save(d);
    group_save(g);

    /*do we need to auto stop when all clients finished voting?*/
  }
  else if (strcmp(scmd, "status") == 0)
  {
    struct vote *v;
    struct list_head *t;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    if (!(v = d->vote.v))
      /* bug or corrupt of network packet */
      return 1;

    REP_ADD(cmd, "OK");

    l = 0;
    list_for_each(t, &v->device_head)
    {
      struct device *m = list_entry(t, struct device, vote.l);
      if (m->vote.choice >= 0)
      {
        LIST_ADD_NUM(buf, l, (int)m->id);
      }
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);
  }
  else if (strcmp(scmd, "showresult") == 0)
  {
    struct vote *v;
    struct list_head *t;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    v = d->vote.v;
    if (!v)
      /* bug or corrupt of network packet */
      return 1;

    /* make reply string */
    REP_ADD(cmd, "OK");

    REP_ADD_NUM(cmd, v->n_members);

    l = 0;
    for (i=0; i<v->cn_options; i++)
    {
      LIST_ADD_NUM(buf, l, v->results[i]);
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);

    /* send the result to all clients about the vote */
    list_for_each(t, &v->device_head)
    {
      struct device *m = list_entry(t, struct device, vote.l);
      sendto_dev_tcp(cmd->rep, cmd->rl, m);
      m->vote.v = NULL;
    }

    d->vote.v = NULL;

    free(v);


    d->db_data->vote_master = 0;
    device_save(d);

    g->vote.current = NULL;
    g->db_data->vote_id = 0;
    g->db_data->vote_results[0] = '0';
    g->db_data->vote_results[1] = 0;
    group_save(g);
  }
  else if (strcmp(scmd, "stop") == 0)
  {
    struct list_head *t;

    REP_OK(cmd);

    list_for_each(t, &g->device_head)
    {
      struct device *m = list_entry(t, struct device, list);
      sendto_dev_tcp(cmd->rep, cmd->rl, m);
    }

    /* clear database snapshot */
    memset(db, 0, sizeof db);
    dbl = 0;
  }
  else if (strcmp(scmd, "remind") == 0)
  {
    struct device *m;
    long id;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atol(p);

    m = get_device(id);
    if (!m)
      return 1;

    sendto_dev_tcp(cmd->rep, cmd->rl, m);
  }
  else if (strcmp(scmd, "forbid") == 0)
  {
    struct device *m;
    long id;
    int fbd;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atol(p);

    m = get_device(id);
    if (!m)
      return 1;

    /* the forbidden id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    fbd = atoi(p);

    m->vote.forbidden = fbd;

    sendto_dev_tcp(cmd->rep, cmd->rl, m);
  }
  else
    return 2;

  return 0;
}

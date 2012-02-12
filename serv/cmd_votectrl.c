#include "cmd_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "include/types.h"
#include "include/debug.h"
#include "dev.h"
#include "devctl.h"
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

#define add_vote_member(g, mid, l) \
{ \
  struct db_device *dd; \
  g->vote.memberids[g->vote.nmembers ++] = mid; \
\
  if( (dd = md_find_device(mid)) ) \
    LIST_ADD(g->vote.membernames, l, dd->user_name); \
  else \
    LIST_ADD(g->vote.membernames, l, "?"); \
}

void group_setup_vote(struct group *g, struct db_vote *dv)
{
  char buf[1024];
  int l;
  char *p;

  g->vote.nmembers = 0;
  g->vote.nvoted = 0;

  strcpy(buf, dv->members);
  l = 0;
  if (strcmp(buf, "all")==0)
  {
    struct device *m;

    /* loop through all within this group */
    /* TODO: make it work in server recovery */
    list_for_each_entry(m, &g->device_head, list)
    {
      add_vote_member(g, m->id, l);
    }
  }
  else
  {
    /* loop through only the specified members */
    IDLIST_FOREACH_p(buf)
    {
      int mid = atoi(p);
      add_vote_member(g, mid, l);
    }
  }

  g->vote.current = dv;
}

static int cmd_votectrl(struct cmd *cmd)
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

    REP_ADD_STR(cmd, buf, l);
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

    /* only one vote could be open at the one time. */
    if( g->vote.current )
    {
      return ERR_OTHER;
    }

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

    group_setup_vote(g, dv);
    g->vote.curr_num = num;

    /* create new vote */
    v = vote_new(dv);
    v->cn_options = dv->options_count;
    v->n_members = 0;

    /* set the vote object on the master device
     * but do not add it to the vote list */
    d->vote.v = v;

    /*send vote start cmd to the members*/

#define device_vote_start() do { \
      send_cmd_to_dev(cmd, m); \
      vote_add_device(v, m); \
      m->vote.v = v; \
      m->vote.choice = -1; \
      m->db_data->vote_choice = -1; \
      } while (0)

    for( i=0 ; i<g->vote.nmembers ; i++ )
    {
      if( (m = get_device(g->vote.memberids[i])) )
      {
        device_vote_start();
      }
      ++ v->n_members;
    }

    /* send to the special 'manager' virtual device */
    send_cmd_to_dev_id(cmd, 1);

    d->db_data->vote_master = 1;
    device_save(d);

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

    g->vote.nvoted ++;

    REP_OK(cmd);

    d->db_data->vote_choice = i;
    vote_results_to_str( g->db_data->vote_results, d->vote.v );
    device_save(d);
    group_save(g);

    /*do we need to auto stop when all clients finished voting?*/
  }
  else if (strcmp(scmd, "status") == 0)
  {
    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    if( !d->vote.v )
      /* bug or corrupt of network packet */
      return 1;

    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, g->vote.nmembers);
    REP_ADD_NUM(cmd, g->vote.nvoted);
    REP_END(cmd);
  }
  else if (strcmp(scmd, "showresult") == 0)
  {
    struct vote *v;
    struct device *m;

    if( !g->vote.current )
    {
      return ERR_OTHER;
    }

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

    REP_ADD_STR(cmd, buf, l);
    REP_END(cmd);

    /* send the result to all clients about the vote */
    list_for_each_entry(m, &v->device_head, vote.l)
    {
      send_cmd_to_dev(cmd, m);
      m->vote.v = NULL;
    }

    /* send to the special 'manager' virtual device */
    send_cmd_to_dev_id(cmd, 1);

    d->vote.v = NULL;

    free(v);


    d->db_data->vote_master = 0;
    device_save(d);

    g->vote.current = NULL;
    g->vote.nmembers = 0;

    g->db_data->vote_id = 0;
    g->db_data->vote_results[0] = '0';
    g->db_data->vote_results[1] = 0;
    group_save(g);
  }
  else if (strcmp(scmd, "stop") == 0)
  {
    struct device *m;

    REP_OK(cmd);

    list_for_each_entry(m, &g->device_head, list)
    {
      send_cmd_to_dev(cmd, m);
    }

    /* clear database snapshot */
    memset(db, 0, sizeof db);
    dbl = 0;
  }
  else if (strcmp(scmd, "remind") == 0)
  {
    struct device *m;
    int id;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atoi(p);

    m = get_device(id);
    if (!m)
      return 1;

    send_cmd_to_dev(cmd, m);
  }
  else if (strcmp(scmd, "forbid") == 0)
  {
    struct device *m;
    int id;
    int fbd;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atoi(p);

    m = get_device(id);
    if (!m)
      return 1;

    /* the forbidden id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    fbd = atoi(p);

    m->vote.forbidden = fbd;

    send_cmd_to_dev(cmd, m);
  }
  else
    return 2;

  return 0;
}

CMD_HANDLER_SETUP(votectrl);

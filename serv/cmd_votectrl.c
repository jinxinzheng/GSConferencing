#include "cmd_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "include/types.h"
#include "dev.h"
#include "sys.h"
#include "db/md.h"

int handle_cmd_votectrl(struct cmd *cmd)
{
  char *scmd, *p;
  char buf[1024];
  int ai=0, i,j,l;

  static struct db_vote *db[1024];
  static int dbl;

  struct device *d;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

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
      fprintf(stderr, "invalid vote select number.\n");
      return 1;
    }

    REP_ADD(cmd, "OK");

    p = db[i]->members;
    REP_ADD(cmd, p);

    REP_END(cmd);
  }
  else if (strcmp(scmd, "start") == 0)
  {
    struct vote *v;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    i = atoi(p);
    if (i >= dbl) {
      fprintf(stderr, "invalid vote start number.\n");
      return 1;
    }

    REP_ADD(cmd, "OK");

    j = db[i]->type;
    REP_ADD_NUM(cmd, j);

    REP_END(cmd);

    /* create new vote */
    v = (struct vote *)malloc(sizeof (struct vote));
    memset(v, 0, sizeof (struct vote));
    INIT_LIST_HEAD(&v->device_head);
    v->cn_options = db[i]->options_count;

    /* set the vote object on the master device
     * but do not add to the vote list */
    d->vote.v = v;

    /*send vote start cmd to the members*/
    strcpy(buf, db[i]->members);
    if (strcmp(buf, "all")==0)
    {
      struct group *g;
      struct list_head *t;

      g = d->group;

#define device_vote_start() do { \
      sendto_dev_tcp(cmd->rep, cmd->rl, d); \
      vote_add_device(v, d); \
      d->vote.v = v; \
      d->vote.choice = -1; \
      } while (0)

      /*send to all within this group*/
      list_for_each(t, &g->device_head)
      {
        d = list_entry(t, struct device, list);
        device_vote_start();
      }
    }
    else
    {
      /*send to only the specified members*/
      IDLIST_FOREACH_p(buf)
      {
        d = get_device(atoi(p));
        if (d) {
          device_vote_start();
        }
      }
    }

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
      d = list_entry(t, struct device, vote.l);
      if (d->vote.choice >= 0)
      {
        LIST_ADD_NUM(buf, l, (int)d->id);
      }
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);
  }
  else if (strcmp(scmd, "showresult") == 0)
  {
    struct group *g;
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

    l = 0;
    for (i=0; i<v->cn_options; i++)
    {
      LIST_ADD_NUM(buf, l, v->results[i]);
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);

    /* send the result to all clients about the vote */
    g = d->group;
    list_for_each(t, &v->device_head)
    {
      d = list_entry(t, struct device, vote.l);
      sendto_dev_tcp(cmd->rep, cmd->rl, d);
      d->vote.v = NULL;
    }
    free(v);
  }
  else if (strcmp(scmd, "stop") == 0)
  {
    struct group *g;
    struct list_head *t;

    REP_OK(cmd);

    if (!d->vote.v)
      /* bug or corrupt of network packet */
      return 1;

    g = d->group;
    list_for_each(t, &g->device_head)
    {
      d = list_entry(t, struct device, list);
      sendto_dev_tcp(cmd->rep, cmd->rl, d);
    }

    /* clear database snapshot */
    memset(db, 0, sizeof db);
    dbl = 0;
  }
  else if (strcmp(scmd, "remind") == 0)
  {
    struct device *d;
    long id;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atol(p);

    d = get_device(id);
    if (!d)
      return 1;

    sendto_dev_tcp(cmd->rep, cmd->rl, d);
  }
  else if (strcmp(scmd, "forbid") == 0)
  {
    struct device *d;
    long id;
    int fbd;

    REP_OK(cmd);

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    id = atol(p);

    d = get_device(id);
    if (!d)
      return 1;

    /* the remindee id */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    fbd = atoi(p);

    d->vote.forbidden = fbd;

    sendto_dev_tcp(cmd->rep, cmd->rl, d);
  }
  else
    return 2;

  return 0;
}

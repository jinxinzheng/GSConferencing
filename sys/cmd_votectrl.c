#include "cmd_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "dev.h"
#include "sys.h"

int handle_cmd_votectrl(struct cmd *cmd)
{
  char *scmd, *p;
  char buf[1024];
  int ai=0, i,j,l;

  /* todo: replace with database */
  static struct {
    char *name;
    char *members;
    int type;
    int cn_options;
  }
  db[] = {
    {"vote1", "101,102,103,104", VOTE_YESNO, 2},
    {"vote2", "101,103,105,106", VOTE_SAT, 5},
    {"vote3", "all", VOTE_CUSTOM, 0},
    {NULL}
  };
  static int dbl = 3;

  scmd = cmd->args[ai++];
  if (!scmd)
    return 1;

  if (strcmp(scmd, "query") == 0)
  {
    REP_ADD(cmd, "OK");

    l=0;
    for (i=0; i<dbl; i++)
    {
      LIST_ADD(buf, l, db[i].name);
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

    p = db[i].members;
    REP_ADD(cmd, p);

    REP_END(cmd);
  }
  else if (strcmp(scmd, "start") == 0)
  {
    struct device *d;
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

    d = get_device(cmd->device_id);
    if (!d)
      return 1;

    j = db[i].type;
    REP_ADD_NUM(cmd, j);

    REP_END(cmd);

    /* create new vote */
    v = (struct vote *)malloc(sizeof (struct vote));
    memset(v, 0, sizeof (struct vote));
    INIT_LIST_HEAD(&v->device_head);
    v->cn_options = db[i].cn_options;

    /* set the vote object on the master device
     * but do not add to the vote list */
    d->vote.v = v;

    /*send vote start cmd to the members*/
    strcpy(buf, db[i].members);
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
      p = strtok(buf, ",");
      while(p)
      {
        d = get_device(atoi(p));
        if (d) {
          device_vote_start();
        }
        p = strtok(NULL, ",");
      }
    }

  }
  else if (strcmp(scmd, "result") == 0)
  {
    struct device *d;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    /* the vote result */
    p = cmd->args[ai++];
    if (!p)
      return 1;
    i = atoi(p);

    d = get_device(cmd->device_id);
    if (!d)
      return 1;

    if (!d->vote.v)
      /* bug or corrupt of network packet */
      return 1;

    /* update results */
    d->vote.choice = i;
    d->vote.v->results[i]++;

    REP_OK(cmd);

    /*do we need to auto stop when all clients finished voting?*/
  }
  else if (strcmp(scmd, "stop") == 0)
  {
    struct device *d;
    struct group *g;
    struct vote *v;
    struct list_head *t;

    /* the vote number */
    p = cmd->args[ai++];
    if (!p)
      return 1;

    d = get_device(cmd->device_id);
    if (!d)
      return 1;

    v = d->vote.v;
    if (!v)
      /* bug or corrupt of network packet */
      return 1;

    l = 0;
    for (i=0; i<v->cn_options; i++)
    {
      LIST_ADD_NUM(buf, l, v->results[i]);
    }
    LIST_END(buf, l);

    REP_ADD(cmd, buf);
    REP_END(cmd);

    /* send the result to all clients within the group */
    g = d->group;
    list_for_each(t, &g->device_head)
    {
      d = list_entry(t, struct device, list);
      sendto_dev_tcp(cmd->rep, cmd->rl, d);
    }
  }
  else
    return 2;

  return 0;
}

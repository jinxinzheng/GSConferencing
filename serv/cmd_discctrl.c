#include "cmd_handler.h"
#include <string.h>
#include "dev.h"
#include "sys.h"
#include "db/md.h"

#define SEND_TO_GROUP_ALL(cmd) \
do { \
  struct group *_g; \
  struct device *_d; \
  struct list_head *_t; \
  if (!(_d = get_device((cmd)->device_id))) \
    return 1; \
  _g = _d->group; \
  list_for_each(_t, &_g->device_head) \
  { \
    _d = list_entry(_t, struct device, list); \
    sendto_dev_tcp((cmd)->rep, (cmd)->rl, _d); \
  } \
} while(0)

#define MAKE_STRLIST(buf, parr, arrlen, member) \
do { \
  int _i, _l=0; \
  for (_i=0; _i<(arrlen); _i++) \
  { \
    LIST_ADD(buf, _l, (parr)[_i]->member); \
  } \
  LIST_END(buf, _l); \
} while(0)

static struct db_discuss *db[1024];
static int dbl;
static struct {
  struct db_discuss *discuss;
  struct list_head dev_list;
} current;

int handle_cmd_discctrl(struct cmd *cmd)
{
  char *subcmd, *p;
  char buf[1024];
  int a=0;
  int i,l;

  struct device *d;

#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) \
    return 1;

  NEXT_ARG(subcmd);

  if (0) ;

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)

  SUBCMD("query")
  {
    dbl = md_get_array_discuss(db);

    REP_ADD(cmd, "OK");

    MAKE_STRLIST(buf, db, dbl, name);
    REP_ADD(cmd, buf);

    REP_END(cmd);
  }

  SUBCMD("select")
  {
    struct db_discuss *s;
    NEXT_ARG(p);
    i = atoi(p);
    s = db[i];

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, s->members);
    REP_END(cmd);

    current.discuss = s;
    INIT_LIST_HEAD(&current.dev_list);

    strcpy(buf, s->members);
    p = strtok(buf, ",");
    do
    {
      if (d = get_device(i))
      {
        list_add_tail(&d->discuss.l, &current.dev_list);
        d->discuss.open = 0;
        d->discuss.forbidden = 0;
        sendto_dev_tcp(cmd->rep, cmd->rl, d);
      }
    }
    while (p = strtok(NULL, ","));
  }

  SUBCMD("request")
  {
    REP_OK(cmd);

    if (!(d = get_device(cmd->device_id)))
      return 1;

    d->discuss.open = 1;
  }

  SUBCMD("status")
  {
    struct list_head *t;
    NEXT_ARG(p);

    REP_ADD(cmd, "OK");

    l = 0;
    list_for_each(t, &current.dev_list)
    {
      d = list_entry(t, struct device, discuss.l);
      if (d->discuss.open)
      {
        LIST_ADD_NUM(buf, l, (int)d->id);
      }
    }
    LIST_END(buf, l);
    REP_ADD(cmd, buf);

    REP_END(cmd);
  }

  SUBCMD("close")
  {
    REP_OK(cmd);

    current.discuss = NULL;
    INIT_LIST_HEAD(&current.dev_list);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("forbid")
  {
    NEXT_ARG(p);
    i = atoi(p);

    REP_OK(cmd);

    if (d = get_device(i))
    {
      d->discuss.forbidden = 1;
      sendto_dev_tcp(cmd->rep, cmd->rl, d);
    }
  }

  SUBCMD("stop")
  {
    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);

    memset(db, 0 ,sizeof db);
    dbl = 0;
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

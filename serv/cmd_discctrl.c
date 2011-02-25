#include "cmd_handler.h"
#include <string.h>
#include "dev.h"
#include "sys.h"
#include "db/md.h"
#include <include/types.h>
#include <include/debug.h>

static struct db_discuss *db[1024];
static int dbl = 0;
static struct {
  struct db_discuss *discuss;
  struct list_head open_list;
  int maxuser;
  int openuser;
} current = {
  0
};

int handle_cmd_discctrl(struct cmd *cmd)
{
  char *subcmd, *p;
  char buf[1024];
  int a=0;
  int i,l;

  struct device *d;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0) ;

  SUBCMD("setmode")
  {
    NEXT_ARG(p);
    i = atoi(p);

    d->group->discuss.mode = i;

    REP_OK(cmd);
  }

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
    if( i>=dbl )
    {
      return ERR_OTHER;
    }
    s = db[i];

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, s->members);
    REP_END(cmd);

    current.discuss = s;
    INIT_LIST_HEAD(&current.open_list);
    current.maxuser = 8;
    current.openuser = 0;

    strcpy(buf, s->members);
    IDLIST_FOREACH_p(buf)
    {
      if (d = get_device(atoi(p)))
      {
        //list_add_tail(&d->discuss.l, &current.dev_list);
        d->discuss.open = 0;
        d->discuss.forbidden = 0;
        sendto_dev_tcp(cmd->rep, cmd->rl, d);
      }
    }
  }

  SUBCMD("request")
  {
    int open;

    if( !current.discuss )
      return ERR_OTHER;

    NEXT_ARG(p);
    open = atoi(p);

    REP_OK(cmd);

    if( open )
    {
      if( current.openuser < current.maxuser )
      {
        /* can direct put through */
      }
      else
      {
        /* opened users reached max count.
         * need to find out a way to handle. */
        if( d->group->discuss.mode == DISCMODE_FIFO )
        {
          struct list_head *t = current.open_list.next;
          struct device *kick = list_entry(t, struct device, discuss.l);
          /* kick one out */
          list_del(t);
          current.openuser --;
          kick->discuss.open = 0;
          /* TODO: should notify the kicked user? */
        }
        else
        {
          trace("too many open users\n");
          return ERR_REJECTED;
        }
      }

      list_add_tail(&d->discuss.l, &current.open_list);
      current.openuser ++;

      tag_add_outstanding(d->tag, d);
    }
    else /* closing */
    {
      /* remove it from the open users list */
      list_del(&d->discuss.l);
      current.openuser --;

      tag_rm_outstanding(d->tag, d);
    }

    d->discuss.open = open;
  }

  SUBCMD("status")
  {
    if( !current.discuss )
      return ERR_OTHER;

    REP_ADD(cmd, "OK");

    list_TO_NUMLIST (buf,
      &current.open_list, struct device, discuss.l, id);
    REP_ADD(cmd, buf);

    REP_END(cmd);
  }

  SUBCMD("close")
  {
    REP_OK(cmd);

    current.discuss = NULL;
    INIT_LIST_HEAD(&current.open_list);
    current.openuser = 0;

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

  SUBCMD("disableall")
  {
    struct group *g;

    NEXT_ARG(p);
    i = atoi(p);

    /* we assume this cmd is only sent from the 'chairman' device */
    g = d->group;
    g->chairman = d;
    g->discuss.disabled = i;

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

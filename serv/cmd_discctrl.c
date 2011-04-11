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
  struct tag *tag;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  tag = d->tag;

  NEXT_ARG(subcmd);

  if (0) ;

  SUBCMD("setmode")
  {
    NEXT_ARG(p);
    i = atoi(p);

    d->tag->discuss.mode = i;

    REP_OK(cmd);

    send_to_tag_all(cmd, d->tag);
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
    int memberids[1024];
    int nmembers=0;

    NEXT_ARG(p);
    i = atoi(p);
    if( i>=dbl )
    {
      return ERR_OTHER;
    }
    s = db[i];

    REP_ADD(cmd, "OK");

    REP_ADD(cmd, s->members);

    /* get user names */
    {
      char tmp[2048];
      struct db_device *dbd;
      int mid;

      strcpy(buf, s->members);
      l = 0;
      IDLIST_FOREACH_p(buf)
      {
        mid = atoi(p);
        memberids[nmembers++] = mid;

        if( dbd = md_find_device(mid) )
        {
          LIST_ADD(tmp, l, dbd->user_name);
        }
        else
        {
          LIST_ADD(tmp, l, "?");
        }
      }
      LIST_END(tmp, l);

      REP_ADD(cmd, tmp);
    }

    REP_END(cmd);

    current.discuss = s;
    INIT_LIST_HEAD(&tag->discuss.open_list);
    tag->discuss.openuser = 0;

    for( i=0 ; i<nmembers ; i++ )
    {
      if (d = get_device(memberids[i]))
      {
        d->discuss.forbidden = 0;
        if( d->discuss.open )
        {
          /* we may restart from a crash */
          d->discuss.open = 0;
          tag_rm_outstanding(d->tag, d);
        }
        sendto_dev_tcp(cmd->rep, cmd->rl, d);
      }
    }
  }

  SUBCMD("request")
  {
    int open;

    // allow test opening
    //if( !current.discuss )
    //  return ERR_OTHER;

    NEXT_ARG(p);
    open = atoi(p);

    if( d->discuss.open == open )
    {
      fprintf(stderr, "dev is already open/closed\n");
      return ERR_OTHER;
    }

    REP_OK(cmd);

    if( open )
    {
      if( tag->discuss.openuser < tag->discuss.maxuser )
      {
        /* can direct put through */
      }
      else
      {
        /* opened users reached max count.
         * need to find out a way to handle. */
        if( d->tag->discuss.mode == DISCMODE_FIFO )
        {
          struct list_head *t = tag->discuss.open_list.next;
          struct device *kick = list_entry(t, struct device, discuss.l);
          /* kick one out */
          list_del(t);
          tag->discuss.openuser --;
          kick->discuss.open = 0;

          tag_rm_outstanding(kick->tag, kick);

          /* notify the kicked user */
          l = sprintf(buf, "%d discctrl kick %d\n", (int)d->id, (int)kick->id);
          sendto_dev_tcp(buf, l, kick);
        }
        else
        {
          trace("too many open users\n");
          return ERR_REJECTED;
        }
      }

      list_add_tail(&d->discuss.l, &tag->discuss.open_list);
      tag->discuss.openuser ++;

      tag_add_outstanding(d->tag, d);
    }
    else /* closing */
    {
      /* remove it from the open users list */
      list_del(&d->discuss.l);
      tag->discuss.openuser --;

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
      &tag->discuss.open_list, struct device, discuss.l, id);
    REP_ADD(cmd, buf);

    REP_END(cmd);
  }

  SUBCMD("close")
  {
    REP_OK(cmd);

    current.discuss = NULL;
    INIT_LIST_HEAD(&tag->discuss.open_list);
    tag->discuss.openuser = 0;

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

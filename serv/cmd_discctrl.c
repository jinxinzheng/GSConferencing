#include "cmd_handler.h"
#include <string.h>
#include "dev.h"
#include "devctl.h"
#include "sys.h"
#include "db/md.h"
#include "ptc.h"
#include <include/types.h>
#include <include/debug.h>

static struct db_discuss *db[1024];
static int dbl = 0;

void group_setup_discuss(struct group *g, struct db_discuss *s)
{
  char buf[1024];
  int l;
  char *p;
  struct db_device *dbd;
  int mid;

  /* setup members */

  g->discuss.nmembers = 0;

  strcpy(buf, s->members);
  l = 0;
  IDLIST_FOREACH_p(buf)
  {
    mid = atoi(p);
    g->discuss.memberids[g->discuss.nmembers ++] = mid;

    if( (dbd = md_find_device(mid)) )
    {
      LIST_ADD(g->discuss.membernames, l, dbd->user_name);
    }
    else
    {
      LIST_ADD(g->discuss.membernames, l, "?");
    }
  }

  g->discuss.current = s;
}

void add_open(struct tag *t, struct device *d)
{
  list_add_tail(&d->discuss.l, &t->discuss.open_list);
  t->discuss.openuser ++;

  d->discuss.open = 1;

  tag_add_outstanding(t, d);

  d->db_data->discuss_open = 1;
  device_save(d);

  /* update interp */
  d->tag->interp.curr_dev = d;
}

void del_open(struct tag *t, struct device *d)
{
  list_del(&d->discuss.l);
  t->discuss.openuser --;

  d->discuss.open = 0;

  tag_rm_outstanding(t, d);

  d->db_data->discuss_open = 0;
  device_save(d);

  /* do we need to set interp curr_dev to null?
   * no! we need it there to do the audio dup. */
}

static void kick_user(struct device *d, struct device *kick)
{
  char buf[256];
  int l;

  trace_info("kick %ld\n", kick->id);

  l = sprintf(buf, "%d discctrl kick %d\n", (int)d->id, (int)kick->id);
  device_cmd(kick, buf, l);
}

static int cmd_discctrl(struct cmd *cmd)
{
  char *subcmd, *p;
  char buf[1024];
  int a=0;
  int i;

  struct device *d;
  struct tag *tag;
  struct group *g;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  tag = d->tag;
  g = d->group;

  NEXT_ARG(subcmd);

  if (0) ;

  SUBCMD("setmode")
  {
    NEXT_ARG(p);
    i = atoi(p);

    d->tag->discuss.mode = i;

    REP_OK(cmd);

    send_to_tag_all(cmd, d->tag);

    g->db_data->discuss_mode = i;
    group_save(g);
  }

  SUBCMD("set_maxuser")
  {
    int max;
    struct tag *t;

    NEXT_ARG(p);
    max = atoi(p);

    /* only support setting maxuser of tag 1 */
    t = get_tag(TAGUID(g->id,1));
    t->discuss.maxuser = max;

    REP_OK(cmd);

    g->db_data->discuss_maxuser = max;
    group_save(g);
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
    int num;

    /* only one discuss could open. */
    if( g->discuss.current )
    {
      return ERR_OTHER;
    }

    NEXT_ARG(p);
    num = atoi(p);
    if( num>=dbl )
    {
      return ERR_OTHER;
    }
    s = db[num];

    group_setup_discuss(g, s);

    g->discuss.curr_num = num;

    REP_ADD(cmd, "OK");

    REP_ADD(cmd, s->name);

    REP_ADD(cmd, s->members);

    REP_ADD(cmd, g->discuss.membernames);

    REP_END(cmd);


    INIT_LIST_HEAD(&tag->discuss.open_list);
    tag->discuss.openuser = 0;

    for( i=0 ; i<g->discuss.nmembers ; i++ )
    {
      struct device *m;
      if( (m = get_device(g->discuss.memberids[i])) )
      {
        m->discuss.forbidden = 0;
        send_cmd_to_dev(cmd, m);
      }
    }

    /* send to the special 'manager' virtual device */
    send_cmd_to_dev_id(cmd, 1);

    d->db_data->discuss_chair = 1;
    device_save(d);

    g->chairman = d;

    g->db_data->discuss_id = s->id;
    group_save(g);
  }

  SUBCMD("request")
  {
    int open;

    // allow test opening
    //if( !current.discuss )
    //  return ERR_OTHER;

    NEXT_ARG(p);
    open = atoi(p);

    REP_ADD(cmd, "OK");

    if( d->discuss.open == open )
    {
      trace_warn("dev is already open/closed\n");
      if( open )
      {
        /* restore the open state. */
        tag_add_outstanding(tag, d);
        ptc_push(d);

        /* return the info for the net mixer if present */
        if( d->mixer.info )
          REP_ADD(cmd, d->mixer.info);
        REP_END(cmd);
        return 0;
      }
      else
      {
        /* if it's already closed. just return ok. */
        REP_END(cmd);
        return 0;
      }
    }

    if( open )
    {
      if( d->type == DEVTYPE_CHAIR && tag->discuss.openuser < MAX_MIX )
      {
        goto openit;
      }

      if( tag->discuss.openuser < tag->discuss.maxuser )
      {
        /* can direct put through */
      }
      else
      {
        /* opened users reached max count.
         * need to find out a way to handle. */
        struct device *k, *kick = NULL;
        int opened = tag->discuss.openuser;

        /* chair users should not occupy our site */
        list_for_each_entry(k, &tag->discuss.open_list, discuss.l)
        {
          if( k->type == DEVTYPE_CHAIR )
          {
            -- opened;
            if( opened < tag->discuss.maxuser )
              goto openit;
          }
          else
          {
            /* candidate to kick in fifo mode */
            if(!kick)
              kick = k;
          }
        }

        if( d->tag->discuss.mode == DISCMODE_FIFO )
        {
          if( !kick )
          {
            /* can't find one to kick */
            return ERR_REJECTED;
          }

          /* kick one out */
          del_open(tag, kick);

          /* notify the kicked user */
          kick_user(d, kick);
        }
        else
        {
          trace_warn("too many open users\n");
          return ERR_REJECTED;
        }
      }

openit:
      add_open(tag, d);

      /* send the pantilt control cmd */
      ptc_push(d);
    }
    else /* closing */
    {
      /* remove it from the open users list */
      del_open(tag, d);

      /* remove it from the pantilt list */
      ptc_remove(d);
    }

    /* return the info for the (net) mixer if present */
    if( d->mixer.info )
      REP_ADD(cmd, d->mixer.info);
    REP_END(cmd);
  }

  SUBCMD("status")
  {
    if( !g->discuss.current )
      return ERR_OTHER;

    REP_ADD(cmd, "OK");

    list_TO_NUMLIST (buf,
      &tag->discuss.open_list, struct device, discuss.l, id);
    REP_ADD(cmd, buf);

    REP_END(cmd);
  }

  SUBCMD("close")
  {
    if( !g->discuss.current )
    {
      return ERR_OTHER;
    }

    REP_OK(cmd);

    g->discuss.current = NULL;
    INIT_LIST_HEAD(&tag->discuss.open_list);
    tag->discuss.openuser = 0;

    SEND_TO_GROUP_ALL(cmd);

    d->db_data->discuss_chair = 0;
    device_save(d);

    g->chairman = NULL;

    g->db_data->discuss_id = 0;
    group_save(g);
  }

  SUBCMD("forbid")
  {
    NEXT_ARG(p);
    i = atoi(p);

    REP_OK(cmd);

    if( (d = get_device(i)) )
    {
      d->discuss.forbidden = 1;
      send_cmd_to_dev(cmd, d);
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
    NEXT_ARG(p);
    i = atoi(p);

    /* we assume this cmd is only sent from the 'chairman' device */
    g->chairman = d;
    g->discuss.disabled = i;

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(discctrl);

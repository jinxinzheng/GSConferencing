#include "cmd_handler.h"
#include <string.h>
#include "dev.h"
#include "devctl.h"
#include "sys.h"
#include "db/md.h"
#include "ptc.h"
#include <include/types.h>
#include <include/debug.h>
#include <include/lock.h>
#include "brcmd.h"
#include "incbuf.h"
#include "interp.h"

static struct db_discuss *db[1024];
static int dbl = 0;

void group_setup_discuss(struct group *g, struct db_discuss *s)
{
  int l;
  char *p;
  struct db_device *dbd;
  int mid;

  /* setup members */

  g->discuss.nmembers = 0;

  l = 0;
  FOREACH_ID(p, s->members)
  {
    mid = atoi(p);
    g->discuss.memberids[g->discuss.nmembers ++] = mid;

    INC_BUF(g->discuss.membernames, g->discuss.membernames_size, l);
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
  /* locking is needed as we could be calling
   * these in recover.c and hbeat.c, etc. */
  LOCK(t->discuss.lk);
  list_add_tail(&d->discuss.l, &t->discuss.open_list);
  t->discuss.openuser ++;

  d->discuss.open = 1;

  tag_add_outstanding(t, d);

  d->db_data->discuss_open = 1;
  device_save(d);
  UNLOCK(t->discuss.lk);
}

void del_open(struct tag *t, struct device *d)
{
  LOCK(t->discuss.lk);
  list_del(&d->discuss.l);
  t->discuss.openuser --;

  d->discuss.open = 0;

  tag_rm_outstanding(t, d);

  d->db_data->discuss_open = 0;
  device_save(d);
  UNLOCK(t->discuss.lk);
}

static void kick_user(struct device *d, struct device *kick)
{
  char buf[256];
  int l;

  trace_info("kick %d\n", kick->id);

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

    brcast_cmd_to_tag_all(cmd, d->tag->id);

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
    REP_END(cmd);

    /* notify multi members, short version. */
    brcast_cmd_to_multi(cmd, g->discuss.memberids, g->discuss.nmembers);

    /* reply to the sender and manager, long version. */
    cmd->rl --; //cancel REP_END
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
      }
      else
      {
        /* if it's already closed. just return ok. */
      }
      goto post;
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

        list_for_each_entry(k, &tag->discuss.open_list, discuss.l)
        {
          if( k->type == DEVTYPE_CHAIR )
          {
            /* chair users should not occupy our site */
            -- opened;
            if( opened < tag->discuss.maxuser )
              goto openit;
          }
          else if( !k->active )
          {
            /* we can directly kick a dead dev */
            kick = k;
            break;
          }
          else
          {
            /* candidate to kick in fifo mode */
            if(!kick)
              kick = k;
          }
        }

        if( !kick )
        {
          /* can't find one to kick */
          return ERR_REJECTED;
        }

        if( !kick->active || d->tag->discuss.mode == DISCMODE_FIFO )
        {
          /* kick one out */
          del_open(tag, kick);
          ptc_remove(kick);

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

post:
    if( d->type == DEVTYPE_INTERP )
    {
      if( tag->interp.mode > 0 )
      {
        /* interp is replicate mode */
        if( open )
        {
          /* exit replicate */
          interp_del_rep(tag);
        }
        else
        {
          /* enter replicate */
          apply_interp_mode(tag, d);
        }
      }
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

    brcast_cmd_to_all(cmd);

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

    brcast_cmd_to_all(cmd);

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

    brcast_cmd_to_all(cmd);
  }

  SUBCMD("demand")
  {
    int did;
    NEXT_ARG(p);
    did = atoi(p);

    NEXT_ARG(p);

    if( d->id != 0 )
    {
      /* only allow manager to demand open/close */
      return ERR_OTHER;
    }

    REP_OK(cmd);

    send_cmd_to_dev_id(cmd, did);
  }

  else
    return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(discctrl);

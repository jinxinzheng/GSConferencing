#include "cmd_handler.h"
#include "include/types.h"
#include "db/md.h"
#include "devctl.h"

static inline void regist_notify_all(struct cmd *cmd, struct group *g)
{
  struct device *d;

  list_for_each_entry(d, &g->device_head, list)
  {
    /* clear regist state each time
     * regist is started/stopped */
    d->regist.reg = 0;

    d->db_data->regist_reg = d->regist.reg;
    device_save(d);

    send_cmd_to_dev(cmd, d);
  }
}

static int cmd_regist(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  struct device *d;
  struct group *g;
  char *p;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  g = d->group;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("start")
  {
    if( g->db_data->regist_start )
    {
      return ERR_OTHER;
    }

    NEXT_ARG(p);

    g->regist.expect =
      g->stats.dev_count[DEVTYPE_NORMAL] +
      g->stats.dev_count[DEVTYPE_CHAIR];
    g->regist.arrive = 0;

    g->db_data->regist_expect = g->regist.expect;
    g->db_data->regist_arrive = g->regist.arrive;

    g->db_data->regist_start = 1;
    g->db_data->regist_mode = atoi(p);
    group_save(g);

    d->db_data->regist_master = 1;
    device_save(d);

    REP_OK(cmd);

    regist_notify_all(cmd, g);
  }

  SUBCMD("stop")
  {
    if( !g->db_data->regist_start )
    {
      return ERR_OTHER;
    }

    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, g->regist.expect);
    REP_ADD_NUM(cmd, g->regist.arrive);
    REP_END(cmd);

    regist_notify_all(cmd, g);

    g->regist.expect = 0;
    g->regist.arrive = 0;

    g->db_data->regist_expect = g->regist.expect;
    g->db_data->regist_arrive = g->regist.arrive;

    g->db_data->regist_start = 0;
    group_save(g);

    d->db_data->regist_master = 0;
    device_save(d);
  }

  SUBCMD("status")
  {
    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, g->regist.expect);
    REP_ADD_NUM(cmd, g->regist.arrive);
    REP_END(cmd);
  }

  SUBCMD("reg")
  {
    int mode;
    int cid;
    struct db_device *dd;
    const char *uname;

    NEXT_ARG(p);
    mode = atoi(p);

    NEXT_ARG(p);
    cid = atoi(p);

    if( !g->db_data->regist_start ||
         g->db_data->regist_mode != mode )
    {
      return ERR_OTHER;
    }

    if( !(dd = d->db_data) )
    {
      return ERR_OTHER;
    }

    if( REGIST_CARD_ID == mode )
    {
      if( cid != dd->user_card )
      {
        return ERR_OTHER;
      }
    }

    /* work around empty user name issue */
    uname = dd->user_name;
    uname += strspn(uname, " ");
    if( uname[0] == 0 )
    {
      uname = ".";
    }

    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, dd->user_card);
    REP_ADD    (cmd, uname);
    REP_ADD_NUM(cmd, dd->user_gender);
    REP_END(cmd);

    g->regist.arrive ++;

    g->db_data->regist_arrive = g->regist.arrive;
    group_save(g);

    d->regist.reg = 1;

    d->db_data->regist_reg = 1;
    device_save(d);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(regist);

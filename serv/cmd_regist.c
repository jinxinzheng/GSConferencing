#include "cmd_handler.h"
#include "include/types.h"
#include "db/md.h"

int handle_cmd_regist(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  char buf[1024];
  struct device *d;
  struct group *g;
  char *p;
  int i;

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

    g->db_data->regist_start = 1;
    g->db_data->regist_mode = atoi(p);
    group_save(g);

    d->db_data->regist_master = 1;
    device_save(d);

    g->regist.expect = md_get_device_count();

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("stop")
  {
    if( !g->db_data->regist_start )
    {
      return ERR_OTHER;
    }

    g->db_data->regist_start = 0;
    group_save(g);

    d->db_data->regist_master = 0;
    device_save(d);

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
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

    d->regist.reg = 1;
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

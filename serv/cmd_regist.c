#include "cmd_handler.h"
#include "include/types.h"
#include "db/md.h"
#include "devctl.h"
#include "brcmd.h"

static void regist_setup(struct group *g, struct db_regist *dr)
{
  char *p;
  g->regist.nmembers = 0;
  FOREACH_ID(p, dr->members)
  {
    int mid = atoi(p);
    g->regist.memberids[g->regist.nmembers ++] = mid;
  }
}

static void regist_reset_all(struct cmd *cmd, struct group *g)
{
  struct device *d;
  int i;
  for( i=0; i<g->regist.nmembers; i++ )
  {
    if( !(d=get_device(g->regist.memberids[i])) )
      continue;
    /* clear regist state each time
     * regist is started/stopped */
    d->regist.reg = 0;
    d->db_data->regist_reg = d->regist.reg;
    /* saving the state for each device is causing
     * performance problem.. */
    //device_save(d);
  }
}

//static void *reg_start_brcmd;

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
    int mode;
    iter it;
    struct db_regist *dr;

    if( g->db_data->regist_start )
    {
      return ERR_OTHER;
    }

    it = NULL;
    if( !(dr = md_iterate_regist_next(&it)) )
      return ERR_OTHER;

    regist_setup(g, dr);
    regist_reset_all(cmd, g);

    NEXT_ARG(p);
    mode = atoi(p);

    g->regist.mode = mode;
    g->regist.expect = dr->expect;
    g->regist.arrive = 0;

    if( !g->regist.arrive_idstr )
      g->regist.arrive_idstr = malloc(16*1024);
    g->regist.arrive_idstr[0] = 0;
    g->regist.arrive_idstr_len = 0;

    g->db_data->regist_expect = g->regist.expect;
    g->db_data->regist_arrive = g->regist.arrive;

    g->db_data->regist_start = 1;
    g->db_data->regist_mode = mode;
    group_save(g);

    d->db_data->regist_master = 1;
    device_save(d);

    REP_OK(cmd);

    send_cmd_to_dev_id(cmd, 1);

    brcast_cmd_to_multi(cmd, g->regist.memberids, g->regist.nmembers);
  }

  SUBCMD("stop")
  {
    if( !g->db_data->regist_start )
    {
      return ERR_OTHER;
    }

    /* broadcast short version. */
    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, g->regist.expect);
    REP_ADD_NUM(cmd, g->regist.arrive);
    REP_END(cmd);
    brcast_cmd_to_all(cmd);

    /* reply long version. */
    cmd->rl --; //cancel REP_END
    REP_ADD(cmd, g->regist.arrive_idstr);
    REP_END(cmd);

    send_cmd_to_dev_id(cmd, 1);

    regist_reset_all(cmd, g);

    g->regist.nmembers = 0;
    g->regist.expect = 0;
    g->regist.arrive = 0;

    g->regist.arrive_idstr_len = 0;

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
    if( d->id == 0 )
    {
      REP_ADD(cmd, g->regist.arrive_idstr);
    }
    REP_END(cmd);
  }

  SUBCMD("reg")
  do {
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

    if( d->regist.reg )
    {
      /* duplicate regist. can be due to issue from client side.
       * we do not accumulate it, but still need to reply info
       * to the client. */
      break;
    }

    g->regist.arrive_ids[g->regist.arrive ++] = d->id;

    LIST_ADD_NUM(g->regist.arrive_idstr, g->regist.arrive_idstr_len, d->id);

    g->db_data->regist_arrive = g->regist.arrive;
    //group_save(g);
    /* TODO: save arrive_ids. */

    d->regist.reg = 1;

    d->db_data->regist_reg = 1;
    //device_save(d);
  } while(0);

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(regist);

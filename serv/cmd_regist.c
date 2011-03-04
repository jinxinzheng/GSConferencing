#include "cmd_handler.h"
#include "include/types.h"
#include "db/md.h"

static struct {
  int start;
  int mode;
  int expect;
  int arrive;
}
regist ={
  0, 0, 0, 0,
};

int handle_cmd_regist(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  char buf[1024];
  struct device *d;
  char *p;
  int i;

  /* require reg() before any cmd */
  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("start")
  {
    NEXT_ARG(p);
    regist.mode = atoi(p);

    regist.start = 1;
    regist.expect = md_get_device_count();
    regist.arrive = 0;

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("stop")
  {
    regist.start = 0;

    REP_OK(cmd);

    SEND_TO_GROUP_ALL(cmd);
  }

  SUBCMD("status")
  {
    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, regist.expect);
    REP_ADD_NUM(cmd, regist.arrive);
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

    if( !regist.start ||
        regist.mode != mode )
    {
      return ERR_OTHER;
    }

    if( !(dd=md_find_device(d->id)) )
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

    regist.arrive ++;
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

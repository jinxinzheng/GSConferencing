#include "cmd_handler.h"
#include "include/types.h"

static struct {
  int start;
  int mode;
  int expect;
  int arrive;
  struct {
    int id;
    char name[64];
    int gender;
  } users[100];
}
regist ={
  0, 0, 0, 0,
  {
    {1, "ma", 1 },
    {2, "mb", 1 },
    {3, "mc", 1 },
    {4, "md", 1 },
    {5, "me", 1 },
    {6, "wa", 0 },
    {7, "wb", 0 },
    {8, "wc", 0 },
    {9, "wd", 0 },
    {10,"we", 0 }
  }
};

int handle_cmd_regist(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  char buf[1024];
  struct device *d;
  char *p;
  int i;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("start")
  {
    regist.start = 1;

    NEXT_ARG(p);
    regist.mode = atoi(p);

    regist.expect = 10;
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

  SUBCMD("by_key")
  {
    if( regist.mode != REGIST_KEY )
    {
      return ERR_OTHER;
    }
    REP_OK(cmd);

    regist.arrive ++;
  }

  SUBCMD("by_card")
  {
    if( regist.mode != REGIST_CARD_ANY )
    {
      return ERR_OTHER;
    }
    REP_OK(cmd);

    regist.arrive ++;
  }

  SUBCMD("by_card_id")
  {
    int cid;
    NEXT_ARG(p);
    cid = atoi(p);

    if( regist.mode != REGIST_CARD_ID )
    {
      return ERR_OTHER;
    }

    for (i=0; i<regist.expect; i++)
    {
      if (regist.users[i].id == cid)
      {
        break;
      }
    }
    if( i>regist.expect )
    {
      return ERR_OTHER;
    }

    REP_ADD(cmd, "OK");
    REP_ADD(cmd, regist.users[i].name);
    REP_ADD_NUM(cmd, regist.users[i].gender);
    REP_ADD_NUM(cmd, regist.users[i].id);
    REP_END(cmd);

    regist.arrive ++;
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

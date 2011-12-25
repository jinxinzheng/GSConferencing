#include "cmd_handler.h"
#include "db/db.h"

static int authenticate(const char *card_id, const char *card_info, char *extra)
{
  struct db_auth_card dc;

  strcpy(dc.card_id, card_id);

  if( db_get_auth_card_by_card_id(&dc, 1) <= 0 )
    return 0;

  if( strcmp(card_info, dc.card_info) != 0 )
  {
    return 0;
  }

  strcpy(extra, dc.extra);

  return 1;
}

static int cmd_auth(struct cmd *cmd)
{
  int a=0;

  char *card_id;
  char *card_info;
  char extra[256];

  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(card_id);
  NEXT_ARG(card_info);

  if( !authenticate(card_id, card_info, &extra[0]) )
  {
    return ERR_REJECTED;
  }

  REP_ADD(cmd, "OK");
  REP_ADD(cmd, extra);
  REP_END(cmd);

  return 0;
}

CMD_HANDLER_SETUP(auth);

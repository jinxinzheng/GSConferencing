#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "strhash.h"

static struct cmd_handler_entry *cmdhandler_hash[HASH_SZ] = {0};

void init_cmd_handlers()
{
}

void register_cmd_handler(struct cmd_handler_entry *ent)
{
  if( find_by_id(cmdhandler_hash, ent->id) )
  {
    fprintf(stderr, "bug! cmd %s already registered.", ent->id);
    return;
  }

  hash_id(cmdhandler_hash, ent);
}

int handle_cmd(struct cmd *cmd)
{
  struct cmd_handler_entry *he;

  he = find_by_id(cmdhandler_hash, cmd->cmd);
  if (!he)
    return -1;

  return he->handler(cmd);
}

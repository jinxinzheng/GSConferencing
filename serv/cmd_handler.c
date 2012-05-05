#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "strhash.h"
#include <include/lock.h>

static struct cmd_handler_entry *cmdhandler_hash[HASH_SZ] = {0};

static pthread_mutex_t cmdhandler_lk = PTHREAD_MUTEX_INITIALIZER;

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
  int r;

  he = find_by_id(cmdhandler_hash, cmd->cmd);
  if (!he)
    return -1;

  LOCK(cmdhandler_lk);
  r = he->handler(cmd);
  UNLOCK(cmdhandler_lk);
  return r;
}

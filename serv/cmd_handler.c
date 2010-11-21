#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "strhash.h"

static struct cmd_handler_entry *cmdhandler_hash[HASH_SZ];

#define CMD_HANDLER_INIT(c) {#c, handle_cmd_##c, NULL, NULL}

static struct cmd_handler_entry cmdhandlers[] = {
  CMD_HANDLER_INIT(reg),
  CMD_HANDLER_INIT(sub),
  CMD_HANDLER_INIT(convctrl),
  CMD_HANDLER_INIT(votectrl),
  {NULL}
};

void init_cmd_handlers()
{
  int i;
  struct cmd_handler_entry *he;

  memset(cmdhandler_hash, 0, sizeof cmdhandler_hash);

  for (i=0; cmdhandlers[i].id; i++)
  {
    he = cmdhandlers + i;
    hash_id(cmdhandler_hash, he);
  }
}

int handle_cmd(struct cmd *cmd)
{
  struct cmd_handler_entry *he;

  he = find_by_id(cmdhandler_hash, cmd->cmd);
  if (!he)
    return -1;

  return he->handler(cmd);
}
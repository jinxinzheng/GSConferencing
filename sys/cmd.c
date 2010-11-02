#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cmd.h"
#include "cmd_handler.h"
#include "strhash.h"

static struct cmd_handler_entry *cmdhandler_hash[HASH_SZ];

#define CMD_HANDLER_INIT(c) {#c, handle_cmd_##c, NULL, NULL}

static struct cmd_handler_entry cmdhandlers[] = {
  CMD_HANDLER_INIT(reg),
  CMD_HANDLER_INIT(sub),
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

int parse_cmd(char *line, struct cmd *pcmd)
{
  char *p;
  int i;

  p = strtok(line, " \r\n");
  pcmd->device_id = atoi(p);

  p = strtok(NULL, " \r\n");
  if (!p)
    return 1;
  pcmd->cmd=p;

  i=0;
  do
  {
    p = strtok(NULL, " \r\n");
    pcmd->args[i++] = p;
  }
  while (p);

  return 0;
}

int handle_cmd(struct cmd *cmd)
{
  struct cmd_handler_entry *he;

  he = find_by_id(cmdhandler_hash, cmd->cmd);
  if (!he)
    return -1;

  return he->handler(cmd);
}

#include <string.h>
#include "cmd.h"

/* command names are arranged in accordance with CMD_* */
static const char *cmds[] = {
  "reg",
  "sub",
  NULL
};

int parse_cmd(char *line, struct cmd *pcmd)
{
  char *p;
  int i;

  p = strtok(line, " \r\n");
  pcmd->device_id = atoi(p);

  p = strtok(NULL, " \r\n");
  if (!p)
    return 1;
  pcmd->cmd=0;
  /* linear search */
  for (i=0; cmds[i]; i++)
  {
    if (strcasecmp(p, cmds[i]) == 0)
      pcmd->cmd = i+1;
  }
  if (pcmd->cmd == 0)
    return 1;

  i=0;
  do
  {
    p = strtok(NULL, " \r\n");
    pcmd->args[i++] = p;
  }
  while (p);

  return 0;
}

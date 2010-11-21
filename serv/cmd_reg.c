#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "devctl.h"

int handle_cmd_reg(struct cmd *cmd)
{
  char *p;
  struct device *newdev;

  p = cmd->args[0];
  if (p && p[0]=='p')
  {
    char *port = p+1;

    newdev = (struct device *)malloc(sizeof (struct device));
    memset(newdev, 0, sizeof (struct device));
    newdev->id = cmd->device_id;
    newdev->addr = *cmd->saddr;
    newdev->addr.sin_port = htons(atoi(port));

    if (dev_register(newdev) != 0)
      free(newdev);
  }
  else
    return 1;
  REP_OK(cmd);
  return 0;
}

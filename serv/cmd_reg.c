#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "devctl.h"
#include "include/cksum.h"
#include "../config.h"

int handle_cmd_reg(struct cmd *cmd)
{
  int a=0;

  char *p;
  struct device *newdev;
  char sum[64];
  unsigned long n;
  int type;
  char *pass;
  int port;
  char *bcast;

  NEXT_ARG(p);
  type = atoi(p);

  NEXT_ARG(p);
  pass = p;

  NEXT_ARG(p);
  port = atoi(p);

  NEXT_ARG(p);
  bcast = p;

  /* authenticate the passwd based on id and type */
  n = (unsigned long)cmd->device_id ^ (unsigned long)type;
  cksum(&n, sizeof n, sum);
  if (strcmp(pass, sum) != 0)
  {
    fprintf(stderr, "authenticate fail\n");
    return 1;
  }

  if (port>0)
  {
    newdev = (struct device *)malloc(sizeof (struct device));
    memset(newdev, 0, sizeof (struct device));
    newdev->id = cmd->device_id;
    newdev->addr = *cmd->saddr;
    newdev->addr.sin_port = htons(port);
    if (strcmp("none", bcast) == 0)
      newdev->bcast.sin_addr.s_addr = 0;
    else
    {
      newdev->bcast.sin_addr.s_addr = inet_addr(bcast);
      newdev->bcast.sin_port = htons(BRCAST_PORT);
    }

    if (dev_register(newdev) != 0)
      free(newdev);
  }
  else
    return 1;
  REP_OK(cmd);
  return 0;
}

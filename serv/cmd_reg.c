#include "cmd_handler.h"
#include <stdlib.h>
#include <string.h>
#include "devctl.h"
#include "include/cksum.h"
#include "../config.h"
#include "db/md.h"

int handle_cmd_reg(struct cmd *cmd)
{
  int a=0;

  char *p;
  struct device *d;
  char sum[64];
  unsigned long n;
  int type;
  char *pass;
  int port;
  char *bcast;

  int did = cmd->device_id;

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

  if (port <= 0)
    return 1;

  d = get_device(did);

  if( !d )
  {
    d = dev_create(did);
  }

  d->addr = *cmd->saddr;
  d->addr.sin_port = htons(port);

  d->fileaddr = d->addr;
  d->fileaddr.sin_port = htons(port+1);

  if (strcmp("none", bcast) == 0)
    d->bcast.sin_addr.s_addr = 0;
  else
  {
    d->bcast.sin_addr.s_addr = inet_addr(bcast);
    d->bcast.sin_port = htons(BRCAST_PORT);
  }

  if (dev_register(d) != 0)
  {
    /* existing dev ok */
  }

  return 0;
}

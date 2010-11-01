#include <string.h>
#include <stdlib.h>
#include "cmd.h"
#include "strhash.h"
#include "dev.h"
#include "devctl.h"
#include "sys.h"

struct cmd_handler_entry
{
  const char *id; /* the cmd name. use 'id' for hashing. */

  int (*handler)(struct cmd *);

  /* ID hash table linkage. */
  struct cmd_handler_entry *hash_next;
  struct cmd_handler_entry **hash_pprev;
};

static struct cmd_handler_entry *cmdhandler_hash[HASH_SZ];

/* cmd handlers */

#define REP_OK(cmd) \
do { \
  strcpy((cmd)->rep+(cmd)->rl, " OK\n"); \
  (cmd)->rl += 4; \
} while(0)

/* handle the cmd.
 * handler should fill the response in cmd->rep. */
int handle_cmd_reg(struct cmd *cmd)
{
  char *p;
  struct device *newdev;

  p = cmd->args[0];
  if (p && p[0]=='p')
  {
    char *port = p+1;

    newdev = (struct device *)malloc(sizeof (struct device));
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

int handle_cmd_sub(struct cmd *cmd)
{
  char *p;

  p = cmd->args[0];

  if (!p)
    return 1;
  else
  {
    struct device *d;
    struct tag *t;
    long tid, gid;
    long long tuid;

    d = get_device(cmd->device_id);

    gid = d->group->id;
    tid = atoi(p);
    tuid = TAGUID(gid, tid);
    t = get_tag(tuid);

    if (d) {
      if (!t)
        /* create an 'empty' tag that has no registered device.
         * this ensures the subscription not lost if there are
         * still not any device of the tag registered. */
        t = tag_create(gid, tid);

      dev_subscribe(d, t);
    }
  }
  REP_OK(cmd);
  return 0;
}


#define CMD_HANDLER_INIT(c) {#c, handle_cmd_##c, NULL, NULL}

static struct cmd_handler_entry cmdhandlers[] = {
  CMD_HANDLER_INIT(reg),
  CMD_HANDLER_INIT(sub),
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

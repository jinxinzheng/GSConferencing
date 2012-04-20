/* broadcast cmd routines */
#include  <include/util.h>
#include  <include/pack.h>
#include  <cmd/cmd.h>
#include  "network.h"

static int sock = 0;
static int seq;

#define INIT_SOCK() { \
  if( !sock ) \
    sock = open_broadcast_sock(); \
}

#define UCMD_SIZE(ucmd) \
  (offsetof(struct pack_ucmd, data) + (ucmd)->datalen)

void brcast_cmd_to_all(struct cmd *cmd)
{
  char buf[CMD_MAX+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  ucmd->type = PACKET_UCMD;
  ucmd->cmd = UCMD_BRCAST_CMD;
  ucmd->u.brcast_cmd.seq = ++ seq;
  ucmd->u.brcast_cmd.mode = BRCMD_MODE_ALL;
  ucmd->datalen = cmd->rl+1;
  memcpy(ucmd->data, cmd->rep, cmd->rl+1);  /* include the ending \0 */

  broadcast_local(sock, ucmd, UCMD_SIZE(ucmd));
}

void brcast_cmd_to_tag_all(struct cmd *cmd, int tagid)
{
  char buf[CMD_MAX+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  ucmd->type = PACKET_UCMD;
  ucmd->cmd = UCMD_BRCAST_CMD;
  ucmd->u.brcast_cmd.seq = ++ seq;
  ucmd->u.brcast_cmd.mode = BRCMD_MODE_TAG;
  ucmd->u.brcast_cmd.tag = tagid;
  ucmd->datalen = cmd->rl+1;
  memcpy(ucmd->data, cmd->rep, cmd->rl+1);  /* include the ending \0 */

  broadcast_local(sock, ucmd, UCMD_SIZE(ucmd));
}

void brcast_cmd_to_multi(struct cmd *cmd, int ids[], int n)
{
  char buf[CMD_MAX*2+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;
  unsigned char *p;
  int l, off;

  INIT_SOCK();

  ucmd->type = PACKET_UCMD;
  ucmd->cmd = UCMD_BRCAST_CMD;
  ucmd->u.brcast_cmd.seq = ++ seq;
  ucmd->u.brcast_cmd.mode = BRCMD_MODE_MULTI;

  l = n*sizeof(ids[0]);

  p = &ucmd->data[0];
  off = 0;
  *(int *)p = n;
  off += sizeof(int);
  memcpy(p+off, ids, l);
  off += l;
  memcpy(p+off, cmd->rep, cmd->rl+1);
  off += cmd->rl+1;

  ucmd->datalen = off;

  broadcast_local(sock, ucmd, UCMD_SIZE(ucmd));
}

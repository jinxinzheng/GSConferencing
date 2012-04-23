/* broadcast cmd routines */
#include  <include/util.h>
#include  <include/pack.h>
#include  <cmd/cmd.h>
#include  "network.h"
#include  <include/lock.h>
#include  <include/debug.h>

static int sock = 0;
static int seq;

static void async_brcast(const void *buf, int len);

#define REPEAT  10

#define INIT_SOCK() { \
  if( !sock ) \
    sock = open_broadcast_sock(); \
}

#define UCMD_SIZE(ucmd) \
  (offsetof(struct pack_ucmd, data) + (ucmd)->datalen)

#define BRCAST_UCMD(ucmd) \
  async_brcast(ucmd, UCMD_SIZE(ucmd))

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

  BRCAST_UCMD(ucmd);
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

  BRCAST_UCMD(ucmd);
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

  BRCAST_UCMD(ucmd);
}

struct brcast_args
{
  int len;
  unsigned char data[1];
};

static pthread_mutex_t brcast_lk = PTHREAD_MUTEX_INITIALIZER;

static void run_brcast(void *arg)
{
  struct brcast_args *args = (struct brcast_args *) arg;
  int i;
  for( i=0 ; i<REPEAT ; i++ )
  {
    /* the socket can be used in multiple threads. */
    LOCK(brcast_lk);
    broadcast_local(sock, args->data, args->len);
    UNLOCK(brcast_lk);
    if( i==REPEAT-1 )
      break;
    else if( i<2 )
      sleep(1);
    else
      sleep(2);
  }
  free(arg);
}

static void async_brcast(const void *buf, int len)
{
  struct brcast_args *args;

  if( len > CMD_MAX-100 )
  {
    trace_err("broadcast data is too long: %d\n", len);
    return;
  }

  args = (struct brcast_args *) malloc(sizeof(struct brcast_args)+len);
  if( !args )
  {
    trace_err("broadcast out of mem\n", len);
    return;
  }

  args->len = len;
  memcpy(args->data, buf, len);

  async_run(run_brcast, args);
}

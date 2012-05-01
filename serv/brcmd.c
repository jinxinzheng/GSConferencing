/* broadcast cmd routines */
#include  <unistd.h>
#include  <include/util.h>
#include  <include/pack.h>
#include  <cmd/cmd.h>
#include  "network.h"
#include  <include/lock.h>
#include  <include/debug.h>
#include  "async.h"

static int sock = 0;
static int seq;

static void async_brcast(const void *buf, int len);
static void *async_brcast_loop(const void *buf, int len);

#define REPEAT  10

#define INIT_SOCK() { \
  if( !sock ) \
    sock = open_broadcast_sock(); \
}

#define UCMD_SIZE(ucmd) \
  (offsetof(struct pack_ucmd, data) + (ucmd)->datalen)

#define BRCAST_UCMD(ucmd) \
  async_brcast(ucmd, UCMD_SIZE(ucmd))

static void make_brcmd_all(struct pack_ucmd *ucmd, struct cmd *cmd)
{
  ucmd->type = PACKET_UCMD;
  ucmd->cmd = UCMD_BRCAST_CMD;
  ucmd->u.brcast_cmd.seq = ++ seq;
  ucmd->u.brcast_cmd.mode = BRCMD_MODE_ALL;
  ucmd->datalen = cmd->rl+1;
  memcpy(ucmd->data, cmd->rep, cmd->rl+1);  /* include the ending \0 */
}

void brcast_cmd_to_all(struct cmd *cmd)
{
  char buf[CMD_MAX+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  make_brcmd_all(ucmd, cmd);

  BRCAST_UCMD(ucmd);
}

void *brcast_cmd_to_all_loop(struct cmd *cmd)
{
  char buf[CMD_MAX+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  make_brcmd_all(ucmd, cmd);

  return async_brcast_loop(ucmd, UCMD_SIZE(ucmd));
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

static void make_brcmd_multi(struct pack_ucmd *ucmd, struct cmd *cmd, int ids[], int n)
{
  unsigned char *p;
  int l, off;

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
}

#define N 256
void brcast_cmd_to_multi(struct cmd *cmd, int ids[], int n)
{
  char buf[CMD_MAX*2+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  int m,i;
  m = n/N;
  for( i=0 ; i<m ; i++ )
  {
    make_brcmd_multi(ucmd, cmd, &ids[i*N], N);
    BRCAST_UCMD(ucmd);
  }
  m = n%N;
  if( m!=0 )
  {
    make_brcmd_multi(ucmd, cmd, &ids[i*N], m);
    BRCAST_UCMD(ucmd);
  }
}

void *brcast_cmd_to_multi_loop(struct cmd *cmd, int ids[], int n)
{
  char buf[CMD_MAX*2+100];
  struct pack_ucmd *ucmd = (struct pack_ucmd *) buf;

  INIT_SOCK();

  make_brcmd_multi(ucmd, cmd, ids, n);

  return async_brcast_loop(ucmd, UCMD_SIZE(ucmd));
}

struct brcast_args
{
  int stop;
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

static struct brcast_args *init_args(const void *buf, int len)
{
  struct brcast_args *args;

  if( len > CMD_MAX-100 )
  {
    trace_err("broadcast data is too long: %d\n", len);
    return NULL;
  }

  args = (struct brcast_args *) malloc(sizeof(struct brcast_args)+len);
  if( !args )
  {
    trace_err("broadcast out of mem\n");
    return NULL;
  }

  args->stop = 0;
  args->len = len;
  memcpy(args->data, buf, len);

  return args;
}

static void async_brcast(const void *buf, int len)
{
  struct brcast_args *args;
  args = init_args(buf, len);
  if( !args )
    return;
  async_run(run_brcast, args);
}

/* run until stop */
static void run_brcast_loop(void *arg)
{
  struct brcast_args *args = (struct brcast_args *) arg;
  while( !args->stop )
  {
    /* the socket can be used in multiple threads. */
    LOCK(brcast_lk);
    broadcast_local(sock, args->data, args->len);
    UNLOCK(brcast_lk);
    sleep(1);
  }
  free(arg);
}

static void *async_brcast_loop(const void *buf, int len)
{
  struct brcast_args *args;
  args = init_args(buf, len);
  if( !args )
    return args;
  async_run(run_brcast_loop, args);
  return args;
}

void brcast_cmd_stop(void *arg)
{
  struct brcast_args *args = (struct brcast_args *) arg;
  if( args )
    args->stop = 1;
}

#include  <unistd.h>
#include  <stdint.h>
#include  <stdlib.h>
#include  <arpa/inet.h>
#include  "client.h"
#include  "net.h"
#include  <include/types.h>
#include  <include/cksum.h>
#include  <include/util.h>
#include  <cmd/t2cmd.h>
#include  <include/pack.h>
#include  <include/hash.h>

#if 0
#define UCAST_LOG   printf
#else
#define UCAST_LOG(...)
#endif

static int tag = 1;

struct dev_ent
{
  struct dev_ent *hash_next;
  struct dev_ent **hash_pprev;

  int id;
  struct sockaddr_in addr;
  int active;
  int sub;  /* set to 0 if unsubbed */
};
static struct dev_ent subs[4096];
static int subs_count = 0;

static HASH(subs_hash, struct dev_ent);

static void add_sub(int id, uint32_t addr, uint16_t port, uint16_t flag)
{
  int i = subs_count;
  struct dev_ent *e = &subs[i];
  e->id = id;
  e->addr.sin_family = AF_INET;
  e->addr.sin_addr.s_addr = addr;
  e->addr.sin_port = port;
  e->active = flag&1;
  e->sub = 1;
  UCAST_LOG("add %d %s:%d %s\n", id, inet_ntoa(e->addr.sin_addr), ntohs(port), e->active?"(a)":"");
  subs_count ++;

  hash_id(subs_hash, e);
}

static int handle_type2_cmd(struct type2_cmd *cmd)
{
  switch ( cmd->cmd )
  {
    case T2CMD_SUBS_LIST :
    {
      struct t2cmd_subs_list *hd = (struct t2cmd_subs_list *) cmd->data;
      int count;
      typeof(hd->subs[0]) *e;
      int i;
      if( hd->tag != tag )
        return -1;
      count = hd->count;
      for( i=0 ; i<count ; i++ )
      {
        e = &hd->subs[i];
        add_sub(e->id, e->addr, e->port, e->flag);
      }
      break;
    }

    case T2CMD_SUB :
    {
      struct t2cmd_sub *args = (struct t2cmd_sub *) cmd->data;
      struct dev_ent *e;
      UCAST_LOG("%d %s %d\n", args->id, args->sub?"sub to":"unsub", args->tag);
      if( args->sub )
      {
        e = find_by_id(subs_hash, (int)args->id);
        if( e )
          e->sub = 1;
        else
          add_sub(args->id, args->addr, args->port, args->flag);
      }
      else
      {
        e = find_by_id(subs_hash, (int)args->id);
        if( e )
          e->sub = 0;
      }
      break;
    }
  }
  return 0;
}

static int audio_sock;

static void init_audio_sock()
{
  if( (audio_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    die("socket()");
}

static int handle_audio_pack(struct pack *p, int len)
{
  static uint32_t seq;
  int i;

  /* re-mangle the pack header */
  p->seq = ++seq;
  P_HTON(p);

  if( (seq&255)==0 )
  {
    UCAST_LOG("cast pack %u. %d\n", seq, len);
  }

  for( i=0 ; i<subs_count ; i++ )
  {
    if( subs[i].active && subs[i].sub )
    {
      sendto(audio_sock, p, len, 0,
        (struct sockaddr *)&subs[i].addr, sizeof(subs[0].addr));
    }
  }
  return 0;
}

static int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_TYPE2_CMD :
    {
      struct type2_cmd *cmd = (struct type2_cmd *) arg1;
      int len = (int) arg2;
      /* sanity checks */
      if( cmd->type != 2 )
        return -1;
      if( len != T2CMD_SIZE(cmd) )
        return -1;
      return handle_type2_cmd(cmd);
    }

    case EVENT_AUDIO_RAW :
    {
      struct pack *p = (struct pack *) arg1;
      int len = (int) arg2;
      handle_audio_pack(p, len);
    }
  }
  return 0;
}

static int run_cast()
{
  while(1)
    sleep(1<<20);
}

int get_subs(int tag);

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 0x1ca00;
  int port = 0x8200;
  int type = DEVTYPE_UCAST_AUDIO;
  unsigned int s;
  char pass[64];
  struct dev_info info;

  while ((opt = getopt(argc, argv, "S:t:")) != -1) {
    switch (opt) {
      case 'S':
        srvaddr = optarg;
        break;
      case 't':
        tag = atoi(optarg);
        break;
    }
  }

  init_audio_sock();

  set_event_callback(on_event);

  id |= tag;
  port |= tag;
  client_init(id, type, srvaddr, port);

  s = (unsigned int)id ^ type;
  cksum(&s, sizeof s, pass);
  if( reg(pass, &info)!=0 )
    return -1;

  if( info.tag != tag )
  {
    if( switch_tag(tag) != 0 )
      return -1;
  }

  if( get_subs(tag)!=0 )
    return -1;

  run_cast();

  return 0;
}

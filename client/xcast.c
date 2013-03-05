#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
#include  "client.h"
#include  "cast.h"
#include  <include/types.h>
#include  <include/pack.h>
#include  <config.h>
#include  "mix.h"
#include  "net.h"
#include  <arpa/inet.h>

static int frame_len;
static char *frame_buf;
static int curr_len = 0;
static struct sockaddr_in copy_dests[100];
static int ncopy;

static void cast_audio(const void *buf, int len)
{
  if( curr_len == 0 )
  {
    frame_buf = get_cast_buffer();
  }
  memcpy(&frame_buf[curr_len], buf, len);
  curr_len += len;
  if( curr_len == frame_len )
  {
    cast_filled(frame_len);
    curr_len = 0;
  }
}

static int mix_audio_pack(struct pack *pack, int len)
{
  int r = put_mix_audio(pack);
  if( r<0 ) /* some error occured */
    return -1;
  else if( r==0 )
  {
    /* the pack does not need mix. */
    cast_audio(pack->data, pack->datalen);
  }
  else
  {
    struct pack *p;
    while( (p=get_mix_audio()) )
    {
      cast_audio(p->data, p->datalen);
    }
  }
  return 0;
}

static int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_AUDIO_RAW:
    {
      struct pack *p = (struct pack *) arg1;
      int len = (int) arg2;
      mix_audio_pack(p, len);
      if( ncopy>0 )
      {
        int i;
        P_HTON(p);
        for( i=0 ; i<ncopy ; i++ )
        {
          send_udp(p, len, &copy_dests[i]);
        }
      }
      break;
    }
  }
  return 0;
}

static void parse_copy_dests(const char *str)
{
  char tmp[512], *p;
  int i, sp, ep;
  strcpy(tmp, str);
  for( p=strtok(tmp, ",") ; p ; p=strtok(NULL, ",") )
  {
    sp = atoi(p);
    if( (p = strchr(p, '-')) )
      ep = atoi(++p);
    else
      ep = sp;
    for( i=sp ; i<=ep ; i++ )
    {
      copy_dests[ncopy].sin_family      = AF_INET;
      copy_dests[ncopy].sin_addr.s_addr = inet_addr("127.0.0.1");
      copy_dests[ncopy].sin_port        = htons(i);
      ncopy++;
    }
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 0x5ca00;
  int type = DEVTYPE_MIX_CAST;
  int port = MIX_CAST_PORT;
  int tag = 1;
  int mode = MODE_BROADCAST;
  int compression = COMPR_NONE;
  int fl = 0;

  while ((opt = getopt(argc, argv, "S:t:m:e:l:T:p:")) != -1) {
    switch (opt) {
      case 'S':
        srvaddr = optarg;
        break;
      case 't':
        tag = atoi(optarg);
        break;
      case 'm':
        switch ( optarg[0] )
        {
          case 'b' : mode = MODE_BROADCAST; break;
          case 'u' : mode = MODE_UNICAST; break;
          case 'm' : mode = MODE_MULTICAST; break;
        }
        break;
      case 'e':
        compression = atoi(optarg);
        break;
      case 'l':
        fl = atoi(optarg);
        break;
      case 'T':
        parse_copy_dests(optarg);
        break;
      case 'p':
        port = atoi(optarg);
        break;
    }
  }

  set_cast_mode(mode);

  frame_len = set_compression(compression);

  /* check user override frame length */
  if( fl > 0 )
  {
    frame_len = fl;
  }

  mix_audio_init();
  mix_audio_auto_close(12);

  set_event_callback(on_event);

  /* wrap id and tag */
  id |= tag;
  client_init(id, type, srvaddr, port);

  while(1)
    sleep(1<<20);

  return 0;
}

#include "client.h"
#include "net.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "cmd/cmd.h"
#include "include/queue.h"
#include "include/pack.h"
#include "include/cfifo.h"
#include "include/debug.h"
#include <include/thread.h>
#include <include/util.h>
#include <include/compiler.h>
#include <include/types.h>
#include <config.h>
#include <version.h>
#include "cyctl.h"
#include "mix.h"
#include "rbudp.h"

//#define USE_SEND_QUEUE
//#define USE_RECV_QUEUE

#define MINRECV 0
#define MAXRECV 30

//#define DEBUG_REPEAT(a...)  fprintf(stderr, "repeat: " a)
#define DEBUG_REPEAT(a...)

static int id;
static int devtype;
static int reged;
static struct sockaddr_in servAddr;
static int listenPort;
static struct sockaddr_in net_mixer_addr;
static struct sockaddr_in ucast_addr;

static struct {
  int audio_use_udp;
  int enable_retransmit;
  int zero_compression;
  int audio_direct_mix;
  int audio_rbudp_send;
  int audio_rbudp_recv;
  int audio_send_ucast;
}
opts = {
  .audio_use_udp = 1,
  .enable_retransmit = 0,
  .zero_compression = 0,
  .audio_direct_mix = 0,
};

static int tag_id;
static int subscription[2] = {0};
static int replicate[2];

static event_cb event_handler;

static char br_addr[32];

static int bcast_audio=0;
static int ucast_audio=0;

#define STREQU(a, b) (strcmp((a), (b))==0)

#define CHECKOK(s) if (!STREQU(s,"OK")) return;

static void handle_cmd(int, int isfile, char *buf, int l);
static void handle_ucmd(struct pack_ucmd *ucmd);

static void udp_recved(char *buf, int l);

static struct cfifo udp_snd_fifo;

static struct cfifo udp_rcv_fifo;

static void *run_heartbeat(void *arg);

static void start_recv_audio_rbudp();
static void audio_rbudp_recved(int tag, void *buf, int len);

#ifdef USE_SEND_QUEUE
static void *run_snd_audio(void *arg);
#endif

#ifdef USE_RECV_QUEUE
static void *run_rcv_audio(void *arg);
#endif

void client_init(int dev_id, int type, const char *servIP, int localPort)
{
  int servPort = SERVER_PORT;
  int netplay=0;
  int recv_br;
  int recv_file;

  printf("daya client %s. build date %s\n", build_rev, build_date);

  id = dev_id;

  devtype = type;

  /* get this network's broadcast address */
  strcpy(br_addr, "none");
  get_broadcast_addr(br_addr);

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  listenPort = localPort;

  memset(&net_mixer_addr, 0, sizeof(net_mixer_addr));
  memset(&ucast_addr, 0, sizeof(ucast_addr));

  /* the special netplay mode,
   * only a few functions are enabled. */
  if( type==DEVTYPE_NETPLAY )
  {
    netplay = 1;
    /* only listen to tag 1 */
    subscription[0] = 1;
  }

  /* broadcast audio mode,
   * tag id is wrapped into the lowest byte of dev_id. */
  if( type==DEVTYPE_BCAST_AUDIO )
  {
    bcast_audio = 1;
    tag_id = dev_id & 0xff;
    if( tag_id==0 )
      tag_id = 1;
  }
  else if( type==DEVTYPE_UCAST_AUDIO )
  {
    ucast_audio = 1;
  }

  if( netplay || ucast_audio )
    recv_br = 0;
  else
    recv_br = 1;

  if( ucast_audio )
    recv_file = 0;
  else
    recv_file = 1;

  /* listen cmds */
  if( !(netplay||bcast_audio) )
    start_recv_tcp(listenPort, handle_cmd, recv_file);

  /* listen udp - audio, video and other */
  if( !bcast_audio )
    start_recv_udp(listenPort, udp_recved, recv_br);

  if( opts.audio_send_ucast )
  {
    /* setup fixed ucast address. */
    ucast_addr.sin_family      = AF_INET;
    ucast_addr.sin_addr.s_addr = inet_addr("192.168.1.82");
    ucast_addr.sin_port        = htons(0x8200|tag_id);
  }

  if( opts.audio_rbudp_send )
  {
    rbudp_init_send(AUDIO_RBUDP_PORT);
  }
  if( opts.audio_rbudp_recv )
  {
    if( !bcast_audio )
    {
      rbudp_init_recv(AUDIO_RBUDP_PORT);
      rbudp_set_recv_tag(0);
      rbudp_set_recv_cb(audio_rbudp_recved);
      start_recv_audio_rbudp();
    }
  }

  /* udp sending and recving queues */

  if(!netplay)
  {
    cfifo_init(&udp_snd_fifo, 8, 11); //256 of size and 2K of element size
    cfifo_enable_locking(&udp_snd_fifo);
  }

  cfifo_init(&udp_rcv_fifo, 8, 11);
  cfifo_enable_locking(&udp_rcv_fifo);

  /* udp sender thread */
  if(!netplay)
  {
#ifdef USE_SEND_QUEUE
    start_thread(run_snd_audio, NULL);
#endif
  }

  if( !bcast_audio )
  {
#ifdef USE_RECV_QUEUE
    start_thread(run_rcv_audio, NULL);
#endif
  }

  if( !(netplay||bcast_audio) )
  {
    start_thread(run_heartbeat, NULL);
  }

  if( opts.audio_direct_mix )
  {
    mix_audio_init();
  }
}

void set_option(int opt, int val)
{
  switch ( opt )
  {
    case OPT_AUDIO_UDP :
    opts.audio_use_udp = val;
    break;

    case OPT_ENABLE_RETRANSMIT :
    opts.enable_retransmit = val;
    break;

    case OPT_ZERO_COMPRESSION:
    opts.zero_compression = val;
    break;

    case OPT_AUDIO_DIRECT_MIX:
    opts.audio_direct_mix = val;
    break;

    case OPT_AUDIO_RBUDP_SEND:
    opts.audio_rbudp_send = val;
    break;

    case OPT_AUDIO_RBUDP_RECV:
    opts.audio_rbudp_recv = val;
    break;

    case OPT_AUDIO_SEND_UCAST:
    opts.audio_send_ucast = val;
    break;
  }
}

void set_event_callback(event_cb cb)
{
  event_handler = cb;
}

/* udp casting */

#define HTON P_HTON
#define NTOH P_NTOH

#define headlen(p)    (offsetof(struct pack, data))
#define pack_size(p)  (offsetof(struct pack, data) + (p)->datalen)

static void *run_heartbeat(void *arg)
{
  struct pack hb;
  int sock;
  int len;

  /* use a different sock than the audio thread */
  if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
      perror("socket()");

  /* Send data to the server */

  hb.id = id;
  hb.seq = 0;
  hb.type = PACKET_HBEAT;
  hb.datalen = 0;

  len = pack_size(&hb);

  HTON(&hb);

  while (1)
  {
    sendto(sock, &hb, len, 0, (struct sockaddr *)&servAddr, sizeof(servAddr));

    /* send heart beat every 3 seconds */
    sleep(3);
  }

  return NULL;
}

static void audio_rbudp_recved(int tag, void *buf, int len)
{
  struct audio_data ad = { buf, len };
  event_handler(EVENT_AUDIO,
      (void*)tag,
      &ad);
}

static void *run_recv_audio_rbudp(void *arg)
{
  rbudp_run_recv();
  return NULL;
}

static void start_recv_audio_rbudp()
{
  start_thread(run_recv_audio_rbudp, NULL);
}

static int mic_open;

static void do_open_mic()
{
  mic_open = 1;
}

static void do_close_mic()
{
  mic_open = 0;

  if( opts.audio_direct_mix )
  {
    struct pack pack;
    pack.type = PACKET_MIC_OP;
    pack.id = (uint32_t)id;
    pack.seq = 0; //1 for open, 0 for close
    pack.tag = tag_id;
    pack.datalen = 0;
    HTON(&pack);
    broadcast_udp(&pack, pack_size(&pack));
  }
}

static void send_pack(struct pack *p);
static void send_pack_len(struct pack *p, int len);

static struct pack *audio_current;

void *send_audio_start()
{
  audio_current = (struct pack *)cfifo_get_in(&udp_snd_fifo);
  return audio_current->data;
}

int send_audio_end(int len)
{
  static int qseq = 0;

  if( len <= 0 )
    return -1;

  if( !mic_open && !bcast_audio )
    return -2;

  if( opts.audio_rbudp_send )
  {
    rbudp_broadcast(tag_id, audio_current->data, len);
    return 0;
  }

  audio_current->type = PACKET_AUDIO;
  audio_current->id = (uint32_t)id;
  audio_current->seq = ++qseq;
  audio_current->tag = tag_id;
  audio_current->datalen = (uint16_t)len;

  trace_verb("%d.%d ", audio_current->id, audio_current->seq);
  DEBUG_TIME_NOW();

#ifdef USE_SEND_QUEUE
  //enque
  cfifo_in_signal(&udp_snd_fifo);
#else
  send_pack(audio_current);
#endif

  return 0;
}

static void send_pack(struct pack *p)
{
  send_pack_len(p, pack_size(p));
}

static void send_pack_len(struct pack *p, int len)
{
  HTON(p);
  if( opts.audio_send_ucast )
  {
    send_udp(p, len, &ucast_addr);
  }
  else if( opts.audio_direct_mix || bcast_audio )
  {
    broadcast_udp(p, len);
  }
  else if( opts.audio_use_udp )
  {
    if( net_mixer_addr.sin_family )
      /* send to the net mixer if we have one */
      send_udp(p, len, &net_mixer_addr);
    else
      /* send to server */
      send_udp(p, len, &servAddr);
  }
  else
    send_audio(p, len);
}

static int __used is_silent(const char *buf, int len)
{
  const short *pcm = (const short *)buf;
  int pcmlen = len>>1;
  int i;
  short top=0, bot=0;

  for( i=0 ; i<pcmlen ; i+=2 )
  {
    if(pcm[i]>top) top = pcm[i];
    if(pcm[i]<bot) bot = pcm[i];
  }

  return (top-bot<250);
}

#ifdef USE_SEND_QUEUE
static void *run_snd_audio(void *arg)
{
  struct pack *qitem;
  int l;
  int sc=0;

  while (1)
  {
    //deque
    cfifo_wait_empty(&udp_snd_fifo);
    qitem = (struct pack *)cfifo_get_out(&udp_snd_fifo);

    if( opts.zero_compression )
    {
      /* compress silence */
      if( is_silent(qitem->data, qitem->datalen) )
        sc ++;
      else
        sc = 0;
    }

    if(sc>30)
    {
      qitem->type = PACKET_AUDIO_ZERO;
      l = headlen(qitem);
    }
    else
      l = pack_size(qitem);

    //send audio
    send_pack_len(qitem, l);

    //free
    cfifo__out(&udp_snd_fifo);
  }

  return NULL;
}
#endif  /* USE_SEND_QUEUE */

struct recent_info
{
  uint32_t *seqs;
  int size;
  int pos;
};

static int recent_test(struct recent_info *recent, uint32_t seq)
{
  int i;
  int mask = recent->size-1;
  int pos = recent->pos;
  for( i=0 ; i<recent->size ; i++ )
  {
    if( seq == recent->seqs[(pos+recent->size-i) & mask] )
    {
      return 1;
    }
  }
  /* add the seq to recent.
   * the list is an 'fifo'. */
  pos = (pos+1) & mask;
  recent->pos = pos;
  recent->seqs[pos] = seq;
  return 0;
}

#define RECENT_SIZE (1<<3)
#define RECENT_MASK (RECENT_SIZE-1)

static int is_recved(uint32_t seq)
{
  static uint32_t recent_seqs[RECENT_SIZE] = {0};
  static struct recent_info recent_recved = {
    .seqs = recent_seqs,
    .size = RECENT_SIZE,
    .pos = 0,
  };

  return recent_test(&recent_recved, seq);
}


STATIC_CFIFO(wait_packs, 2, 11);

static int is_wait_full()
{
  return cfifo_full(&wait_packs);
}

static void put_wait(const struct pack *pack)
{
  void *in = cfifo_get_in(&wait_packs);
  memcpy(in, pack, pack_size(pack));
  cfifo_in(&wait_packs);
}

static const struct pack *get_wait()
{
  const void *out = cfifo_get_out(&wait_packs);
  cfifo_out(&wait_packs);
  return (const struct pack *)out;
}

static uint32_t get_wait_seq()
{
  struct pack *p;
  if( cfifo_empty(&wait_packs) )
    return 0;
  p = (struct pack *)cfifo_get_out(&wait_packs);
  return p->seq;
}

static void req_repeat(int tag, uint32_t seq)
{
  static int sock = -1;
  struct pack req;
  int l;

  DEBUG_REPEAT("%s %d\n", __func__, seq);

  if( sock < 0 )
  {
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    {
      perror("socket()");
      return;
    }
  }

  req.id = id;
  req.seq = seq;
  req.type = PACKET_REPEAT_REQ;
  req.tag = tag;
  req.datalen = 0;

  l = pack_size(&req);

  HTON(&req);

  sendto(sock, &req, l, 0, (struct sockaddr *)&servAddr, sizeof(servAddr));
}

static void enque_pack(const struct pack *pack)
{
  void *in = cfifo_get_in(&udp_rcv_fifo);
  memcpy(in, pack, pack_size(pack));
  cfifo_in_signal(&udp_rcv_fifo);
}

#define MAX_LOST  4

static uint32_t expect_seq = 0;

static void queue_audio_pack(struct pack *pack)
{
  const struct pack *next;
  int tag = pack->tag;
  uint32_t s;

  if( pack->type == PACKET_SEQ_OUTDATE )
  {
    /* current expect_seq is oudated.
     * continue to the waiting packs. */
    DEBUG_REPEAT("outdated %d\n", expect_seq);
    ++ expect_seq;
    goto queue_wait;
  }

  if( pack->seq < expect_seq )
  {
    if( expect_seq - pack->seq < 100 )
      /* this could be recently outdated */
      return;
    else
    {
      /* probably reset */
      trace_warn("drop outdated %d, expect %d\n", pack->seq, expect_seq);
      expect_seq = 0;
    }
  }

  if( !opts.enable_retransmit )
  {
    enque_pack(pack);
    return;
  }

  if( pack->seq == expect_seq || expect_seq == 0 )
  {
    next = pack;
  }
  else
  {
    DEBUG_REPEAT("loss detect: expect=%d, pack=%d\n", expect_seq, pack->seq);
    if( is_wait_full() )
    {
      /* move forward. */
      next = get_wait();
      DEBUG_REPEAT("queue full, go forward from %d\n", next->seq);
      put_wait(pack);
    }
    else
    {
      req_repeat(tag, expect_seq);
      put_wait(pack);
      return;
    }
  }

  /* queue next */
  do {
    /* put a pack in */
    enque_pack(next);

    expect_seq = next->seq+1;

queue_wait:
    /* queue any waiting packs if proper. */
    s = get_wait_seq();
    if(s) { DEBUG_REPEAT("proceed waiting %d\n", s); }
    if( s == 0 )
    {
      /* no waiting */
      break;
    }
    else if( s == expect_seq )
    {
      next = get_wait();
    }
    else
    {
      if( s - expect_seq <= MAX_LOST )
      {
        req_repeat(tag, expect_seq);
        break;
      }
      else
        next = get_wait();
    }
  } while(next);
}

static void outdate_seq(struct pack *pack)
{
  if( expect_seq == pack->seq )
  {
    /* tell the waiting packs to continue.
     * TODO: need refactor. */
    queue_audio_pack(pack);
  }
}

static void write_audio(struct pack *p);

#ifdef  USE_RECV_QUEUE
#define RECV_AUDIO_PACK   queue_audio_pack
#else
#define RECV_AUDIO_PACK   write_audio
#endif

static void audio_recved(struct pack *buf, int len)
{
  struct pack *qitem = buf;

  if( ucast_audio )
  {
    /* for the ucast, the tag is not my sub but my tag. */
    if( qitem->tag == tag_id )
    {
      /* let me access the whole pack. */
      event_handler(0x20001, buf, (void *)pack_size(buf));
    }
    return;
  }

  /* return immediately when we are not subscribed to any */
  if (subscription[0] == 0 && subscription[1] == 0)
    return;

  trace_verb("%d.%d ", qitem->id, qitem->seq);
  DEBUG_TIME_NOW();

  /* broadcasted packet could be sent back */
  //if (qitem->id == id)
  //  return;

  /* broadcasted packet needs to be checked by tag */
  if( replicate[0] || replicate[1] )
  {
    if( qitem->tag != replicate[0] &&
        qitem->tag != replicate[1] )
      return;
  }
  else
  if( qitem->tag != subscription[0] &&
      qitem->tag != subscription[1] )
    return;

  if (pack_size(qitem) <= len)
    ;
  else
  {
    fprintf(stderr, "bug: %d+%d > %d\n", headlen(qitem), qitem->datalen, len);
    return; /*mal pack, drop*/
  }

  if( opts.audio_direct_mix )
  {
    int r = put_mix_audio(qitem);
    if( r<0 )
      return;/* some error occured */
    else if( r==0 )
    {
      /* the pack does not need mix. */
      RECV_AUDIO_PACK(qitem);
    }
    else
    {
      struct pack *p;
      while( (p=get_mix_audio()) )
      {
        RECV_AUDIO_PACK(p);
      }
    }
  }
  else
  {
    /* remove dup packs */
    if( is_recved(qitem->seq) )
    {
      return;
    }
    RECV_AUDIO_PACK(qitem);
  }


  trace_dbg("%d packs in fifo\n", cfifo_len(&udp_rcv_fifo));

  //tmp: do not put in queue, directly play.
  /*if (qitem->type == TYPE_AUDIO)
  {
    event_handler(EVENT_AUDIO,
        (void*)qitem->data,
        (void*)qitem->datalen);
  }*/
}

static void audio_mic_op(struct pack *p)
{
  /* it is only valid for direct mix mode */
  if( !opts.audio_direct_mix )
    return;
  /* it must be from an interested tag */
  if( replicate[0] || replicate[1] )
  {
    if( p->tag != replicate[0] &&
        p->tag != replicate[1] )
      return;
  }
  else
  if( p->tag != subscription[0] &&
      p->tag != subscription[1] )
    return;

  if( p->seq )  //seq is the open state
  {
    // not used
    //mix_audio_open(p->id);
  }
  else
  {
    mix_audio_close(p->id);
  }
}

static void video_recved(struct pack *pack, int len);

static void udp_recved(char *buf, int len)
{
  struct pack *pack;

  pack = (struct pack *)buf;

  /* don't do ntoh for this type */
  if( pack->type == PACKET_UCMD )
  {
    handle_ucmd((struct pack_ucmd *)pack);
    return;
  }

  NTOH(pack);

  switch ( pack->type )
  {
    case PACKET_AUDIO :
    audio_recved(pack, len);
    break;

    case PACKET_SEQ_OUTDATE :
    outdate_seq(pack);
    break;

    case PACKET_VIDEO:
    video_recved(pack, len);
    break;

    case PACKET_MIC_OP:
    audio_mic_op(pack);
    break;
  }
}

static inline void drop_rcv_queue(int count)
{
  int i;
  for( i=0 ; i<count ; ++i )
  {
    cfifo__out(&udp_rcv_fifo);
  }
}

static void write_audio(struct pack *p)
{
  static unsigned int prev=0;

  if( p->seq != prev+1 )
  {
    trace_warn("lost seq %d (%d)\n", prev, p->seq);
  }
  prev = p->seq;

  /* generate event */
  if (p->type == PACKET_AUDIO)
  {
    struct audio_data ad = { p->data, p->datalen };

    event_handler(EVENT_AUDIO,
        (void*)(int)p->tag,
        &ad);
  }
}

#ifdef USE_RECV_QUEUE
static void *run_rcv_audio(void *arg)
{
  struct pack *qitem;
  int qlen;
  static int count=0;

  while (1)
  {
    cfifo_wait_empty(&udp_rcv_fifo);

    /* check periodically */
    count = (count+1) & ((1<<8)-1);
    if( !count )
    {
      qlen = cfifo_len(&udp_rcv_fifo);
      if( qlen > 4 )
      {
        int drop = qlen - 1;
        /* force to drop packets to work around the delay. */
        trace_warn("dropping %d\n", drop);
        drop_rcv_queue(drop);
      }
    }

take1:
    qitem = (struct pack *)cfifo_get_out(&udp_rcv_fifo);

    /* quick drop silence packs */
    qlen = cfifo_len(&udp_rcv_fifo);
    if(qlen > 1)
    {
      if( is_silent(qitem->data, qitem->datalen) )
      {
        trace_warn("dropping 1 silent\n");
        drop_rcv_queue(1);
        if( !cfifo_empty(&udp_rcv_fifo) )
          goto take1;
        else
          continue;
      }
    }

    write_audio(qitem);

    //fprintf(stderr, "recv pack %d\n", qitem->seq);

    cfifo__out(&udp_rcv_fifo);
  }

  return NULL;
}
#endif

#define FRAGSIZE  1400

static int do_send_video(const void *buf, int len)
{
  static int sock = -1;

  if( sock < 0 )
  {
    int val;
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    {
      perror("socket()");
      return -1;
    }
    val = UDP_SOCK_BUFSIZE;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val)) < 0)
    {
      perror("setsockopt SO_SNDBUF fail");
    }
  }

  sendto(sock, buf, len, 0, (struct sockaddr *)&servAddr, sizeof(servAddr));

  return 0;
}

int send_video(const char *buf, int len)
{
  /* fragmentise */
  char data[FRAGSIZE+100];
  struct pack *pack = (struct pack *)data;
  int off;
  int i,n,m;
  int frags;

  static uint8_t fseq = 0;

  if( len<0 )
  {
    return -1;
  }

  n = len / FRAGSIZE; /* frags count minus 1 */
  m = len % FRAGSIZE; /* size of last frag */
  frags = n + (m==0?0:1);

  pack->type = PACKET_VIDEO;
  pack->id = (uint32_t)id;
  HTON(pack);

  fseq++;
  off = 0;

  for( i=0 ; i<n ; i++ )
  {
    /* to fully utilize our small header,
     * tag is used for frame seq id,
     * seq is frame size.
     * first 4 bytes of data is frag offset. */
    pack->tag = fseq;
    pack->seq = htonl(len);
    pack->datalen = htons(sizeof(int)+FRAGSIZE);
    *(uint32_t *)(&pack->data[0]) = htonl(off);
    memcpy(pack->data+sizeof(int), buf+off, FRAGSIZE);

    do_send_video(pack, headlen(pack)+sizeof(int)+FRAGSIZE);

    off += FRAGSIZE;
  }

  if( m!=0 )
  {
    pack->tag = fseq;
    pack->seq = htonl(len);
    pack->datalen = htons(sizeof(int)+m);
    *(uint32_t *)(&pack->data[0]) = htonl(off);
    memcpy(pack->data+sizeof(int), buf+off, m);

    do_send_video(pack, headlen(pack)+sizeof(int)+m);
  }

  return 0;
}

static void video_recved(struct pack *pack, int len)
{
  /* defragment */
  static char *frame;
  static int fsize;
  static int flen;
  static int fseq = -1;

  int size;
  int off;

  /* sanity check */
  if( pack_size(pack) != len )
  {
    trace_err("broken video pack %d<>%d\n", pack_size(pack), len);
    return;
  }

  if( fseq != pack->tag )
  {
    /* start recv for a new frame */
    fseq = pack->tag;
    flen = 0;
  }

  size = pack->seq;
  off = ntohl(*(uint32_t *)&pack->data[0]);

  if( fsize < size )
  {
    fsize = size;
    frame = realloc(frame, fsize);
  }

  memcpy(frame+off, pack->data+sizeof(int), pack->datalen-sizeof(int));
  flen += pack->datalen-sizeof(int);

  if( flen==size )
  {
    /* all fragments complete */
    /* directly notify the playback side.. */
    event_handler(EVENT_VIDEO,
        (void*)frame,
        (void*)flen);

    /* reset */
    fseq = -1;
    flen = 0;
  }
}

/* cmd delegates */

#define BASICS \
  char buf[CMD_MAX]; \
  int __attribute__((unused)) l,i; \
  struct cmd c;

#define PRINTC(fmt, args...) \
  l = sprintf(buf, "%d " fmt "\n", id, ##args);

#define __SEND_CMD() do {\
  l = send_tcp(buf, l, &servAddr); \
  if (l>0) { \
    int _e;  \
    if( buf[0] == 2 ) { \
      /* 'type 2' cmd */  \
      event_handler(0x20000, buf, (void *)l); \
      return 0; \
    } \
    buf[l] = 0; \
    if( (_e = parse_cmd(buf, &c)) != 0 ) { \
      fprintf(stderr, "%s failed: %d\n", __func__, _e); \
      if (_e == ERR_NOT_REG) \
        event_handler(EVENT_NEED_REG, NULL, NULL); \
      return _e; \
    } \
  } \
  else if( l == -2) { \
    event_handler(EVENT_NEED_REG, NULL, NULL); \
    return -2; \
  } \
  else return -1; \
} while (0)

#define SEND_CMD() do { \
  /*if( !reged )  \
    return -1;*/  \
  __SEND_CMD(); \
} while(0)

#define FIND_OK(cmd) ({ \
  int _i; \
  for (_i=0; _i<32 && (cmd).args[_i]; _i++) \
    if (strcmp("OK", (cmd).args[_i]) == 0) \
      break; \
  _i; \
})

/* convert null string to empty */
static inline char *null_safe(char *s)
{
  return s? s:"";
}

/* return: position of 'OK' in the reply */
int send_cmd(char *buf, int len, struct cmd *reply)
{
  int i,l;
  struct cmd c = {.cmd=0};

  l = len;

  SEND_CMD();

  i = FIND_OK(c);

  *reply = c;

  return i;
}

#define SIMPLE_CMD(cmd, subcmd) \
int cmd##_##subcmd() \
{ \
  BASICS; \
  l = sprintf(buf, "%d " #cmd " " #subcmd "\n", id); \
  SEND_CMD(); \
  i = FIND_OK(c); \
  return 0; \
}

#define SIMPLE_CMD_i(cmd, subcmd) \
int cmd##_##subcmd(int arg) \
{ \
  BASICS; \
  l = sprintf(buf, "%d " #cmd " " #subcmd " %d\n", id, arg); \
  SEND_CMD(); \
  i = FIND_OK(c); \
  return 0; \
}

/* checks whether an int is in a int list str */
static inline int int_in_str(int i, char *s)
{
  char *p;

  if( !s )
    return 0;

  p = strtok(s, ",");
  do
  {
    if (atoi(p)==i)
      return 1;
    p=strtok(NULL, ",");
  }while (p);
  return 0;
}

static int _str_to_ints(char *s, int *list)
{
  char *p;
  int i;

  if( !s )
    return 0;

  i = 0;
  p = strtok(s, ",");
  while (p) {
    list[i++] = atoi(p);
    p = strtok(NULL, ",");
  }
  return i;
}

static void str_to_int_list(char *s, int *list)
{
  int l = _str_to_ints(s, list);
  list[l] = 0;
}

#define STR_TO_INT_LIST str_to_int_list

static void int_list_to_str(char *s, int list[])
{
  int i, l=0;
  for (i=0; list[i]; i++) {
    l += sprintf(s+l, "%d", list[i]);
    s[l++] = ',';
  }
  s[l>0? l-1:0] = 0;
}

static int str_split(char *s, char *sp[], const char *delim)
{
  char *p;
  int i;
  i = 0;
  p = strtok(s, delim);
  while (p) {
    sp[i++] = p;
    p = strtok(NULL, delim);
  }
  return i;
}

/* entry list string parse helper */

struct parse_cfg
{
  int ent_size;
  int offset;
  int type;
};

#define PARSE_CFG(etype, emember, type)   \
  {sizeof(etype), (int)(&((etype *)0)->emember), type}

#define PARSE_CFG_INT(etype, emember)   \
  PARSE_CFG(etype, emember, 'i')

#define PARSE_CFG_STR(etype, emember)   \
  PARSE_CFG(etype, emember, 's')

#define PARSE_CFG_END()   {0,0,0}

static int parse_entries(void *ents, char *str, const struct parse_cfg *cfg)
{
  int i,j,l,m;
  char *sp[1024];
  char *em[128];
  const struct parse_cfg *pm;
  char *ent = (char *)ents;

  l = str_split(str, sp, ",");

  for( i=0 ; i<l ; i++ )
  {
    m = str_split(sp[i], em, ":");

    for( pm=cfg,j=0 ; pm->type && j<m ; pm++,j++ )
    {
      switch ( pm->type )
      {
        case 'i':
          *(int *)(ent+pm->offset) = atoi(em[j]);
          break;
        case 's':
          strcpy(ent+pm->offset, em[j]);
          break;
      }
    }

    ent += cfg[0].ent_size;
  }

  return l;
}


static void parse_dev_info(char *str, struct dev_info *info)
{
  char *entries[1000];
  char *keyval[4];
  int i,l;

  info->id = id;

  l = str_split(str, entries, ":");

  for( i=0 ; i<l ; i++ )
  {
    str_split(entries[i], keyval, "=");

#define info_str(m) \
    if( STREQU(keyval[0], #m) ) \
    { \
      strncpy(info->m, keyval[1], sizeof(info->m)); \
      /* ensure the string is null-terminated */  \
      info->m[sizeof(info->m)-1] = 0; \
    } else

#define info_int(m) \
    if( STREQU(keyval[0], #m) ) \
    { \
      info->m = atoi(keyval[1]); \
    } else

#define info_int_list(m) \
    if( STREQU(keyval[0], #m) ) \
    { \
      _str_to_ints(keyval[1], info->m); \
    } else

#define info_end() {}

    info_str(client_version)

    info_str(user_id)

    info_str(user_name)
    info_int(user_gender)

    info_int(tag)

    info_int_list(sub)

    info_int(discuss_mode)
    info_int(discuss_chair)
    info_int(discuss_num)
    info_str(discuss_name)
    info_int(discuss_nmembers)
    info_int_list(discuss_idlist)
    info_str(discuss_userlist)
    info_int(discuss_open)

    info_int(regist_start)
    info_int(regist_master)
    info_int(regist_mode)
    info_int(regist_reg)

    info_int(vote_num)
    info_int(vote_nmembers)
    info_int_list(vote_idlist)
    info_str(vote_userlist)
    info_int(vote_master)
    info_str(vote_name)
    info_int(vote_type)
    info_str(vote_options)
    info_int(vote_total)
    info_int_list(vote_results)
    info_int(vote_choice)

    info_end()
  }
}

int reg(const char *passwd, struct dev_info *info)
{
  BASICS;
  char *p;

#define _MAKE_REG() \
  l = sprintf(buf, "%d reg %d %s %d %s\n", id, devtype, passwd, listenPort, br_addr)

#define _POST_REG() \
{ \
  reged = 1;  \
  /*synctime();*/ \
  tag_id = info->tag; \
  subscription[0] = info->sub[0]; \
  if( subscription[0]==0 && subscription[1]==0 ) \
    subscription[0] = 1; \
  if( opts.audio_rbudp_recv ) \
    rbudp_set_recv_tag(subscription[0]);  \
  if( !opts.audio_use_udp )  \
  { \
    /* connect audio sock. this must be done after  \
     * the reg() is completed. */ \
    struct sockaddr_in serv_audio_addr; \
    serv_audio_addr = servAddr; \
    serv_audio_addr.sin_port = htons(SERVER_PORT+2);  \
    connect_audio_sock(&serv_audio_addr, id); \
  } \
}

  _MAKE_REG();

  __SEND_CMD();

  i = FIND_OK(c);

  p = c.args[i+1];

  parse_dev_info(p, info);

  _POST_REG();

  return 0;
}

static void try_reg_end(char *reply)
{
  int e;
  struct cmd c;
  int i;
  char *p;
  struct dev_info tmp, *info = &tmp;

  if( (e = parse_cmd(reply, &c)) != 0 )
  {
    event_handler(EVENT_REG_OK, (void *)e, NULL);
    return;
  }

  i = FIND_OK(c);

  p = c.args[i+1];

  parse_dev_info(p, info);

  _POST_REG();

  event_handler(EVENT_REG_OK, NULL, info);
}

void start_try_reg(const char *passwd)
{
  char buf[2048];
  int l;

  _MAKE_REG();

  start_try_send_tcp(buf, l, &servAddr, try_reg_end);
}

int report_cyc_ctl()
{
  BASICS;
  PRINTC("%s", __func__);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}


int get_tags(struct tag_info tags[], int *count)
{
  BASICS;
  char *info;

  struct parse_cfg cfg[] = {
    PARSE_CFG_INT(struct tag_info, id),
    PARSE_CFG_STR(struct tag_info, name),
    PARSE_CFG_END()
  };

  PRINTC("%s", __func__);

  SEND_CMD();

  i = FIND_OK(c);

  info = c.args[i+1];

  *count = parse_entries(tags, info, cfg);

  return 0;
}

int sub(int tag)
{
  BASICS;

  if( subscription[0] == tag ||
      subscription[1] == tag )
  {
    return 1;
  }

  if( !subscription[0] )
    i = 0;
  else
    //return 1;
    i = 0;

  l = sprintf(buf, "%d sub %d\n", id, tag);

  SEND_CMD();

  subscription[i] = tag;

  /* check if we have any rep-ed tag */
  {
    int j = FIND_OK(c);
    int rep = atoi(c.args[++j]);
    if( rep>0 )
    {
      replicate[i] = rep;
    }
    else
      replicate[i] = 0;
  }

  /* reset retransmit expect seq */
  expect_seq = 0;

  if( opts.audio_rbudp_recv )
  {
    if( replicate[i] )
      rbudp_set_recv_tag(replicate[i]);
    else
      rbudp_set_recv_tag(subscription[i]);
  }

  return 0;
}

int unsub(int tag)
{
  BASICS;

  if( subscription[0] == tag )
    i = 0;
  else
    return 1;

  l = sprintf(buf, "%d sub -%d\n", id, tag);

  SEND_CMD();

  subscription[i] = 0;
  replicate[i] = 0;

  if( opts.audio_rbudp_recv )
  {
    rbudp_set_recv_tag(0);
  }

  return 0;
}

int switch_tag(int tag)
{
  BASICS;

  l = sprintf(buf, "%d %s %d\n", id, __func__, tag);

  SEND_CMD();

  i = FIND_OK(c);

  tag_id = tag;
  return 0;
}


SIMPLE_CMD_i(regist, start);

int regist_stop(int *expect, int *got)
{
  BASICS;
  PRINTC("regist stop");
  SEND_CMD();
  i = FIND_OK(c);

  *expect = atoi(c.args[++i]);
  *got    = atoi(c.args[++i]);

  return 0;
}

int regist_status(int *expect, int *got)
{
  BASICS;

  l = sprintf(buf, "%d regist status\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  *expect = atoi(c.args[++i]);
  *got    = atoi(c.args[++i]);

  return 0;
}

int regist_reg(int mode, int cardid, int *card, char *name, int *gender)
{
  BASICS;

  l = sprintf(buf, "%d regist reg %d %d\n", id, mode, cardid);

  SEND_CMD();

  i = FIND_OK(c);

  *card   = atoi( c.args[++i]);
  strcpy(name,    c.args[++i]);
  *gender = atoi( c.args[++i]);

  return 0;
}


int discctrl_set_mode(int mode)
{
  BASICS;

  l = sprintf(buf, "%d discctrl setmode %d\n", id, mode);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

SIMPLE_CMD_i(discctrl, set_maxuser);

int discctrl_query(char *disclist)
{
  BASICS;

  l = sprintf(buf, "%d discctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(disclist, null_safe(c.args[i+1]));

  return 0;
}

int discctrl_select(int disc_num, int *idlist, char *users_list)
{
  BASICS;

  l = sprintf(buf, "%d discctrl select %d\n", id, disc_num);

  SEND_CMD();

  i = FIND_OK(c);

  /* skip the name arg */
  ++i;

  STR_TO_INT_LIST(c.args[++i], idlist);

  strcpy(users_list, c.args[++i]);

  return 0;
}

int discctrl_request(int open)
{
  BASICS;

  l = sprintf(buf, "%d discctrl request %d\n", id, open);

  SEND_CMD();

  i = FIND_OK(c);

  if( open )
  {
    char *minfo = c.args[++i];
    if( minfo )
    {
      net_mixer_addr.sin_family      = AF_INET;
      net_mixer_addr.sin_addr.s_addr = inet_addr(minfo);
      net_mixer_addr.sin_port        = htons(AUDIO_PORT);
    }
    do_open_mic();
  }
  else
  {
    memset(&net_mixer_addr, 0, sizeof(net_mixer_addr));
    do_close_mic();
  }

  return 0;
}

int discctrl_status(int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d discctrl status\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

int discctrl_close()
{
  BASICS;

  l = sprintf(buf, "%d discctrl close\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_stop()
{
  BASICS;

  l = sprintf(buf, "%d discctrl stop\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_forbid(int did)
{
  BASICS;

  l = sprintf(buf, "%d discctrl forbid %d\n", id, did);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_disable_all(int flag)
{
  BASICS;

  l = sprintf(buf, "%d discctrl disableall %d\n", id, flag);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}


SIMPLE_CMD_i(interp, set_mode);


int votectrl_query(char *votelist)
{
  BASICS;

  l = sprintf(buf, "%d votectrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(votelist, null_safe(c.args[i+1]));

  return 0;
}

int votectrl_select(int vote_num, int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d votectrl select %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

static inline void get_vote_info_from_cmd(struct cmd *c, int start, struct vote_info *info)
{
  int i = start;
  info->type = atoi(c->args[i++]);
  strcpy(info->name, c->args[i++]);
  info->opt_count = atoi(c->args[i++]);
  info->max_select = atoi(c->args[i++]);
  strcpy(info->options, c->args[i++]);
}

int votectrl_start(int vote_num, struct vote_info *info)
{
  BASICS;

  l = sprintf(buf, "%d votectrl start %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  get_vote_info_from_cmd(&c, i+1, info);

  return 0;
}

int votectrl_result(int vote_num, const char *results)
{
  BASICS;

  l = sprintf(buf, "%d votectrl result %d %s\n", id, vote_num, results);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_status(int vote_num, int *total, int *voted)
{
  BASICS;

  l = sprintf(buf, "%d votectrl status %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  *total = atoi(c.args[++i]);
  *voted = atoi(c.args[++i]);

  return 0;
}

int votectrl_showresult(int vote_num, int *total, int *results)
{
  BASICS;

  l = sprintf(buf, "%d votectrl showresult %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  *total = atoi(c.args[++i]);
  STR_TO_INT_LIST(c.args[++i], results);

  return 0;
}

int votectrl_stop()
{
  BASICS;

  l = sprintf(buf, "%d votectrl stop\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_remind(int did)
{
  BASICS;

  l = sprintf(buf, "%d votectrl remind %d\n", id, did);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_forbid(int did, int flag)
{
  BASICS;

  l = sprintf(buf, "%d votectrl forbid %d %d\n", id, did, flag);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int msgctrl_query(int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d msgctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  str_to_int_list(c.args[i+1], idlist);

  return 0;
}

int servicecall(int request)
{
  BASICS;

  l = sprintf(buf, "%d servicecall %d\n", id, request);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int msgctrl_send(int idlist[], const char *msg)
{
  BASICS;
  char tmp[1024];

  if (!idlist)
    strcpy(tmp, "all");
  else
    int_list_to_str(tmp, idlist);

  l = sprintf(buf, "%d msgctrl send %s %s\n", id, tmp, msg);

  SEND_CMD();

  return 0;
}

int videoctrl_query(char *vidlist)
{
  BASICS;

  l = sprintf(buf, "%d videoctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(vidlist, null_safe(c.args[i+1]));

  return 0;
}

int videoctrl_select(int vid_num, char *path)
{
  BASICS;

  l = sprintf(buf, "%d videoctrl select %d\n", id, vid_num);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(path, c.args[i+1]);

  return 0;
}

int filectrl_query(char *filelist)
{
  BASICS;

  l = sprintf(buf, "%d filectrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(filelist, null_safe(c.args[i+1]));

  return 0;
}

int filectrl_select(int file_num, char *path)
{
  BASICS;

  l = sprintf(buf, "%d filectrl select %d\n", id, file_num);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(path, c.args[i+1]);

  return 0;
}


int server_user_login(const char *user, const char *pswd)
{
  BASICS;
  PRINTC("server_user login %s %s", user, pswd);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}

int server_user_change_passwd(const char *user, const char *new_pswd)
{
  BASICS;
  PRINTC("server_user passwd %s %s", user, new_pswd);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}


int sys_stats(int dev_count[])
{
  BASICS;
  PRINTC("sys_stats");
  SEND_CMD();
  i = FIND_OK(c);

  dev_count[0] = 0;
  dev_count[1] = atoi(c.args[++i]);
  dev_count[2] = atoi(c.args[++i]);
  dev_count[3] = atoi(c.args[++i]);

  return 0;
}

int sysconfig(int cid, const char *cmd)
{
  BASICS;
  PRINTC("sysconfig %d %s", cid, cmd);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}

int set_ptc(int cid, int ptcid, const char *ptcmd)
{
  BASICS;
  PRINTC("set_ptc %d %d %s", cid, ptcid, ptcmd);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}

int set_user_id(int cid, const char *user_id)
{
  BASICS;
  PRINTC("set_user_id %d %s", cid, user_id);
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}

int get_all_devs(dev_ent_t devs[], int *count)
{
  BASICS;
  char *p;

  static struct parse_cfg cfg[] = {
    PARSE_CFG_INT(dev_ent_t, id),
    PARSE_CFG_INT(dev_ent_t, type),
    PARSE_CFG_STR(dev_ent_t, user_id),
    PARSE_CFG_INT(dev_ent_t, online),
    PARSE_CFG_END()
  };

  PRINTC("get_all_devs");
  SEND_CMD();
  i = FIND_OK(c);

  p = c.args[++i];

  *count = parse_entries(devs, p, cfg);

  return 0;
}

int auth(const char *card_id, const char *card_info, char * extra)
{
  BASICS;

  PRINTC("auth %s %s", card_id, card_info);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(extra, c.args[i+1]);

  return 0;
}

int video_start()
{
  BASICS;
  PRINTC("video start 0");
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}

int video_stop()
{
  BASICS;
  PRINTC("video stop");
  SEND_CMD();
  i = FIND_OK(c);
  return 0;
}


int synctime()
{
  BASICS;
  struct timespec s,e,t;

  PRINTC("synctime");

  clock_gettime(CLOCK_REALTIME, &s);

  SEND_CMD();

  clock_gettime(CLOCK_REALTIME, &e);

  i = FIND_OK(c);

  t.tv_sec  = atoi(c.args[++i]);
  t.tv_nsec = atoi(c.args[++i]);

  //adjust the round trip time
  t.tv_sec += (e.tv_sec - s.tv_sec)/2;
  t.tv_nsec += (e.tv_nsec - s.tv_nsec)/2;

  if( clock_settime(CLOCK_REALTIME, &t) < 0 )
  {
    perror("clock_settime");
  }

  fprintf(stderr, "server time %s.%s\n", c.args[i-1], c.args[i]);
  fprintf(stderr, "client time %d.%d\n", (int)t.tv_sec, (int)t.tv_nsec);

  return 0;
}

int test()
{
  BASICS;
  PRINTC("test");
  SEND_CMD();
  return 0;
}

int get_subs(int tag)
{
  BASICS;
  PRINTC("get_subs %d", tag);
  SEND_CMD();
  return 0;
}


static struct
{
  int len;
  char data[1024*100];
} transfile;

static void handle_file(int sock, char *buf, int l)
{
  if (sock)
  {
    if( transfile.len + l >= (int)sizeof(transfile.data) )
      return;
    memcpy(transfile.data+transfile.len, buf, l);
    transfile.len += l;
  }
  else
  {
    /* file transfer complete */
    event_handler(EVENT_FILE, transfile.data, (void*)transfile.len);
    transfile.len = 0;
  }
}

enum
{
  TCP_CMD = 0,
  TCP_FILE = 1,
  TCP_AUDIO = 2,
};

static void __handle_cmd(char *buf, int l);

/* handle recved cmd and generate appropriate events to the client */
static void handle_cmd(int sock, int type, char *buf, int l)
{
  /* this is maintained during each connection.
   * caution: this only supports one thread running tcp recv! */
  static struct
  {
    int sock;

    void (*job)(char *data, int *plen, const char *buf, int l);

    char *data;
    int len;
  }
  contex = { 0, 0, NULL, 0 };

  int isfile;


  if( type == TCP_AUDIO )
  {
    /* pass it to the UDP audio receiver. */
    udp_recved(buf, l);
    return;
  }


  isfile = type;

  if (isfile)
  {
    handle_file(sock, buf, l);
    return;
  }


  if (contex.sock == 0)
  {
    /* new connection */
    contex.sock = sock;
    contex.job = NULL;
  }
  else
  {
    if (contex.sock == sock)
    {
      /* connection consecutive */
      if (contex.job)
        contex.job(contex.data, &contex.len, buf, l);
      return;
    }
    else if (0 == sock)
    {
      /* connection is closing */
      if (contex.job)
        contex.job(contex.data, &contex.len, NULL, 0);
      contex.sock = 0;
      contex.job = NULL;
      contex.data = NULL;
      return;
    }
  }

  __handle_cmd(buf, l);
}

static void __handle_cmd(char *buf, int l)
{
  struct cmd c = {.cmd=0};
  int i;

  if( !reged )
  {
    return;
  }

  if( buf[0] == 2 ) /* 'type 2' cmd */
  {
    event_handler(0x20000, buf, (void *)l);
    return;
  }

  parse_cmd(buf, &c);

  i = 0;

  if (STREQU(c.cmd, "votectrl"))
  {
    int vn;
    char *sub = c.args[i++];
    if (STREQU(sub, "start"))
    {
      struct vote_info info;
      vn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      get_vote_info_from_cmd(&c, i, &info);
      event_handler(EVENT_VOTE_START, (void*)vn, (void*)&info);
    }
    else if (STREQU(sub, "showresult"))
    {
      int nmems;
      int results[256];
      vn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      nmems = atoi(c.args[i++]);
      STR_TO_INT_LIST(c.args[i++], results);
      event_handler(EVENT_VOTE_SHOWRESULT, (void*)nmems, (void*)results);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_STOP, NULL, NULL);
    }
    else if (STREQU(sub, "remind"))
    {
      int did;
      did = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_REMIND, (void*)did, NULL);
    }
    else if (STREQU(sub, "forbid"))
    {
      int did;
      int flag;
      did = atoi(c.args[i++]);
      flag = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_REMIND, (void*)did, (void*)flag);
    }
  }

  else if (STREQU(c.cmd, "discctrl"))
  {
    char *sub = c.args[i++];
    if( STREQU(sub, "setmode") )
    {
      int mode = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_SETMODE, (void*)mode, NULL);
    }
    else if( STREQU(sub, "select") )
    {
      char *name;
      int dn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      name = c.args[i++];
      event_handler(EVENT_DISC_OPEN, (void*)dn, name);
    }
    else if (STREQU(sub, "close"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_CLOSE, NULL, NULL);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_STOP, NULL, NULL);
    }
    else if (STREQU(sub, "forbid"))
    {
      int did = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_FORBID, (void*)did, NULL);
    }
    else if(STREQU(sub, "disableall"))
    {
      int flag = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_FORBIDALL, (void*)flag, NULL);
    }
    else if(STREQU(sub, "kick"))
    {
      int kid = atoi(c.args[i++]);
      if( id == kid )
        event_handler(EVENT_DISC_KICK, NULL, NULL);
    }
    else if(STREQU(sub, "demand"))
    {
      int did = atoi(c.args[i++]);
      int open = atoi(c.args[i++]);
      if( id == did )
        event_handler(EVENT_DISC_DEMAND, (void*)open, NULL);
    }
  }

  else if (STREQU(c.cmd, "interp"))
  {
    char *sub = c.args[i++];
    if( STREQU(sub, "set_mode") )
    {
      int mode = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_INTERP_MODE, (void*)mode, NULL);
    }
  }

  else if (STREQU(c.cmd, "regist"))
  {
    char *sub = c.args[i++];
    if (STREQU(sub, "start"))
    {
      int mode = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_REGIST_START, (void*)mode, NULL);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      int expect = atoi(c.args[i++]);
      int got = atoi(c.args[i++]);
      event_handler(EVENT_REGIST_STOP, (void*)expect, (void*)got);
    }
  }

  else if (STREQU(c.cmd, "msgctrl"))
  {
    char *sub = c.args[i++];

    if (STREQU(sub, "send"))
    {
      int uid = c.device_id;
      char *ids = c.args[i++];
   //   char *msg = c.args[i++];


      /******************************/
      char msg[256];
      int msg_len = 0;
      for(; strcmp(c.args[i],"OK")||c.args[i+1] ;)//不是OK，或未到NULL
        msg_len += sprintf(msg + msg_len, " %s",c.args[i++]);
      /******************************/


      if(STREQU(ids, "all"))
      {
        event_handler(EVENT_BROADCAST_MSG, &uid, msg);
      }
      else
      {
        if (int_in_str(id, ids))
        {
          event_handler(EVENT_MSG, &uid, msg);
        }
      }
    }
  }

  else if (STREQU(c.cmd, "file"))
  {
    /* this is a file transfer. need to handle specially. */
    //int len = atoi(c.args[i++]);
    /* handle file transfer.
     * caution: do not assume the order of the file port and
     * the cmd port. it is random on different archs. */
    transfile.len = 0;
  }

  else if (STREQU(c.cmd, "ptc"))
  {
    char *ptcid, *ptcmd;
    ptcid = c.args[i++];
    ptcmd = c.args[i++];
    event_handler(EVENT_PTC, ptcmd, NULL);
  }

  else if (STREQU(c.cmd, "sysconfig"))
  {
    char *cid, *cmd;
    cid = c.args[i++];
    cmd = c.args[i++];
    CHECKOK(c.args[i++]);

    event_handler(EVENT_SYSCONFIG, (void *)c.device_id, cmd);
  }

  else if( STREQU(c.cmd, "cyc_ctl") )
  {
    char *p = c.args[i++];
    trace_warn("cyclic rescue because of %s\n", p);
    do_cyctl(1);
  }

  else if (STREQU(c.cmd, "set_user_id"))
  {
    char *cid, *user_id;
    cid = c.args[i++];
    user_id = c.args[i++];
    CHECKOK(c.args[i++]);

    event_handler(EVENT_SET_UID, user_id, NULL);
  }

  else if (STREQU(c.cmd, "allow_chair_ctl"))
  {
    char *cid;
    int allow;
    cid = c.args[i++];
    allow = atoi(c.args[i++]);

    event_handler(EVENT_CHAIR_CTL, (void *)allow, NULL);
  }

  else if (STREQU(c.cmd, "upgrade"))
  {
    char *version;
    version = c.args[i++];
    event_handler(EVENT_UPGRADE, version, NULL);
  }
}

static int is_ucmd_recved(uint32_t seq)
{
  static uint32_t ucmd_seqs[8] = {0};
  static struct recent_info recent_ucmd = {
    .seqs = ucmd_seqs,
    .size = 8,
    .pos = 0,
  };

  return recent_test(&recent_ucmd, seq);
}

static void handle_brcast_cmd(struct pack_ucmd *ucmd)
{
  if( is_ucmd_recved(ucmd->u.brcast_cmd.seq) )
  {
    return;
  }

  if( ucmd->u.brcast_cmd.mode == BRCMD_MODE_ALL )
  {
    __handle_cmd((char *)ucmd->data, ucmd->datalen);
  }
  else if( ucmd->u.brcast_cmd.mode == BRCMD_MODE_TAG )
  {
    if( ucmd->u.brcast_cmd.tag == tag_id )
      __handle_cmd((char *)ucmd->data, ucmd->datalen);
  }
  else if( ucmd->u.brcast_cmd.mode == BRCMD_MODE_MULTI )
  {
    int *p = (int *) &ucmd->data[0];
    int n = p[0];
    int i;
    for( i=1 ; i<=n ; i++ )
    {
      if( p[i] == id )
      {
        int l = (1+n)*sizeof(p[0]);
        __handle_cmd((char *)(&ucmd->data[l]), ucmd->datalen-l);
        break;
      }
    }
  }
}

static void handle_ucmd(struct pack_ucmd *ucmd)
{
  switch( ucmd->cmd )
  {
    case UCMD_INTERP_REP_TAG :
    {
      /* note: the server may send this multiple times. */
      int tag = ucmd->u.interp.tag;
      int rep = ucmd->u.interp.rep;
      if( subscription[0]==tag )
        replicate[0] = rep;
      else if( subscription[1]==tag )
        replicate[1] = rep;
      else
        break;

      if( opts.audio_rbudp_recv )
      {
        if( rep )
          rbudp_set_recv_tag(rep);
        else
          rbudp_set_recv_tag(tag);
      }
    }
    break;

    case UCMD_BRCAST_CMD :
    {
      handle_brcast_cmd(ucmd);
      break;
    }
  }
}

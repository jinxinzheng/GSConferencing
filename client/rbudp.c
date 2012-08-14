/* retransmittive broadcast UDP protocol. */
#include  <sys/socket.h>
#include  <arpa/inet.h>
#include  <pthread.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <string.h>
#include  <include/cfifo.h>
#include  <include/thread.h>
#include  <include/util.h>
#include  "rbudp.h"

#if 0
# define RBUDP_DBG(a...)   fprintf(stderr, "rbudp: " a)
#else
# define RBUDP_DBG(a...)  do{ }while(0)
#endif

struct rbudp_packet
{
  uint8_t tag;
  uint8_t type;
  uint16_t seq;
  uint16_t len;
  uint16_t pad;
  char data[1];
}
__attribute__((packed));

enum
{
  RBUDP_PACKET,
  RBUDP_RETRAN,
  RBUDP_REPAIR,
};

#define PACK_SIZE(p)  (offsetof(struct rbudp_packet, data)+(p)->len)

#define REP_SIZE   32
#define REP_MASK   31

static uint16_t seq;
static int snd_sock;
static int rep_sock;
static struct sockaddr_in br_addr;
static char rep_bufs[REP_SIZE][2048];
static int rep_seq[REP_SIZE];
static int rep_count[REP_SIZE];
//static pthread_mutex_t snd_lk;

static void *run_retransmit(void *arg);

void rbudp_init_send(int port)
{
  int val;

  if ((snd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");
  val = 1;
  if (setsockopt(snd_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    die("setsockopt");
  val = 1;
  if (setsockopt(snd_sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
    die("setsockopt");

  if ((rep_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");
  val = 1;
  if (setsockopt(rep_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    die("setsockopt");
  val = 1;
  if (setsockopt(rep_sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
    die("setsockopt");

  br_addr.sin_family = AF_INET;
  br_addr.sin_port = htons((uint16_t)port);
  br_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

  /* create a retransmit thread */
  start_thread(run_retransmit, NULL);
}

#if 0
static int sendto_locked(struct rbudp_packet *p, struct sockaddr_in *addr)
{
  int r;
  pthread_mutex_lock(&snd_lk);
  r = sendto(snd_sock, p, PACK_SIZE(p), 0, (struct sockaddr *)addr, sizeof(*addr));
  pthread_mutex_unlock(&snd_lk);
  return r;
}
#endif

static void *run_retransmit(void *arg)
{
  struct sockaddr_in other;
  size_t otherLen;
  char buf[4096];
  int len;
  struct rbudp_packet *req, *p;

  //struct sched_param param;
  //pthread_t tid = pthread_self();
  //param.sched_priority = 0;
  //pthread_setschedparam(tid, SCHED_FIFO, &param);

  while(1)
  {
    otherLen = sizeof(other);
    if( (len = recvfrom(snd_sock, buf, sizeof buf, 0,
            (struct sockaddr *) &other, &otherLen)) < 0 )
    {
      perror("run_retransmit: recvfrom");
      continue;
    }
    req = (struct rbudp_packet *) buf;
    if( req->type == RBUDP_RETRAN )
    {
      RBUDP_DBG("request for seq %d from %s\n", req->seq, inet_ntoa(other.sin_addr));
      /* find the packet in the cache */
      p = (struct rbudp_packet *) rep_bufs[req->seq & REP_MASK];
      if( p->seq==req->seq )
      {
        int *pseq = &rep_seq[req->seq & REP_MASK];
        int *pcount = &rep_count[req->seq & REP_MASK];
        if( *pseq != req->seq )
        {
          *pseq = req->seq;
          *pcount = 0;
        }
        if( *pcount < 8 )
        {
          //if( sendto_locked(p, &other) < 0 )
          if( sendto(rep_sock, p, PACK_SIZE(p), 0, (struct sockaddr *)&other, sizeof(other)) < 0 )
            perror("run_retransmit: sendto");
        }
        else if( *pcount < 20 )
        {
          //if( sendto_locked(p, &br_addr) < 0 )
          if( sendto(rep_sock, p, PACK_SIZE(p), 0, (struct sockaddr *)&br_addr, sizeof(br_addr)) < 0 )
            perror("run_retransmit: sendto");
        }
        ++ (*pcount);
      }
      else
        RBUDP_DBG("can't fulfill request for %d, we have %d\n", req->seq, p->seq);
    }
  }
}

int rbudp_broadcast(int tag, const void *buf, int len)
{
  struct rbudp_packet *p;
  if( ++seq==0 )
    seq=1;
  /* cache the packcet for retransmit */
  p = (struct rbudp_packet *) rep_bufs[seq & REP_MASK];
  p->tag = tag;
  p->type = RBUDP_PACKET;
  p->seq = seq;
  p->len = len;
  memcpy(p->data, buf, len);
  //if( sendto_locked(p, &br_addr) < 0 )
  if( sendto(snd_sock, p, PACK_SIZE(p), 0, (struct sockaddr *)&br_addr, sizeof(br_addr)) < 0 )
  {
    perror("rpudp_broadcast: sendto");
    return -1;
  }
  /* make it useful for retransmision. */
  p->type = RBUDP_REPAIR;
  return 0;
}


static int rcv_sock;
static int rcv_tag;
static uint16_t expect_seq;
STATIC_CFIFO(wait_packs, 3, 11);
static rbudp_recv_cb_t recv_cb;

void rbudp_init_recv(int port)
{
  struct sockaddr_in addr;
  int val;

  if ((rcv_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");
  val = 1;
  if (setsockopt(rcv_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    die("setsockopt");
  val = 1;
  if (setsockopt(rcv_sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
    die("setsockopt");

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(rcv_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    die("bind");
}

void rbudp_set_recv_tag(int tag)
{
  rcv_tag = tag;
  expect_seq = 0;
  RBUDP_DBG("recv tag is now %d\n", tag);
}

void rbudp_set_recv_cb(rbudp_recv_cb_t cb)
{
  recv_cb = cb;
}

static inline int is_wait_full()
{
  return cfifo_full(&wait_packs);
}

static inline int is_wait_empty()
{
  return cfifo_empty(&wait_packs);
}

static void put_wait(const struct rbudp_packet *pack)
{
  void *in = cfifo_get_in(&wait_packs);
  memcpy(in, pack, PACK_SIZE(pack));
  cfifo_in(&wait_packs);
}

static struct rbudp_packet *get_wait()
{
  if( is_wait_empty() )
    return NULL;
  const void *out = cfifo_get_out(&wait_packs);
  cfifo_out(&wait_packs);
  return (struct rbudp_packet *)out;
}

static void clear_wait()
{
  cfifo_clear(&wait_packs);
}

static void req_repeat(int seq, struct sockaddr_in *src)
{
  struct rbudp_packet req;
  int l;
  RBUDP_DBG("%s %d\n", __func__, seq);

  req.tag = rcv_tag;
  req.type = RBUDP_RETRAN;
  req.seq = seq;
  req.len = 0;

  l = PACK_SIZE(&req);

  if( sendto(rcv_sock, &req, l, 0, (struct sockaddr *)src, sizeof(*src)) < 0 )
    perror("req_repeat: sendto");
}

int rbudp_run_recv()
{
  struct sockaddr_in other, src;
  size_t otherLen;
  char buf[4096];
  int len;
  struct rbudp_packet *p, *next;

  while(1)
  {
    otherLen = sizeof(other);
    if( (len = recvfrom(rcv_sock, buf, sizeof buf, 0,
            (struct sockaddr *) &other, &otherLen)) < 0 )
    {
      perror("recvfrom");
      continue;
    }
    p = (struct rbudp_packet *) buf;
    if( p->tag != rcv_tag )
    {
      continue;
    }

    /* don't use repair address */
    if( p->type == RBUDP_PACKET )
    {
      src = other;
    }

    if( p->seq < expect_seq )
    {
      if( expect_seq - p->seq < 100 )
        continue;
      else
      {
        clear_wait();
        expect_seq = 0;
      }
    }
    else if( p->seq > expect_seq )
    {
      if( is_wait_empty() )
      {
        if( p->seq - expect_seq > 1 )
        {
          expect_seq = p->seq;
        }
      }
    }

    if( p->seq == expect_seq || expect_seq==0 )
    {
      next = p;
    }
    else
    {
      if( is_wait_full() )
      {
        /* move forward. */
        next = get_wait();
        RBUDP_DBG("move forward from %d\n", next->seq);
        put_wait(p);
      }
      else
      {
        if( is_wait_empty() )
          req_repeat(expect_seq, &src);
        put_wait(p);
        continue;
      }
    }

    do {
      /* commit a pack */
      recv_cb(rcv_tag, next->data, next->len);
      expect_seq = next->seq+1;

      /* queue any waiting packs. */
      next = get_wait();
      if( next )
        RBUDP_DBG("fast forward %d\n", next->seq);
    } while(next);
  }
}

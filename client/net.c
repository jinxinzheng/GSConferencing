#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <include/util.h>
#include <include/thread.h>
#include "include/encode.h"
#include <include/compiler.h>
#include "../config.h"
#include "net.h"

#include <include/debug.h>

#define BUFLEN 10240

static void (*udp_recved)(char *buf, int l);
static void (*tcp_recved)(int sock, int isfile, char *buf, int l);

void get_broadcast_addr(char *addr)
{
  struct ifaddrs *ifa, *p;

  getifaddrs(&ifa);

  for(p=ifa; p; p=p->ifa_next)
  {
    if (strcmp("eth0", p->ifa_name) == 0)
    {
      if (p->ifa_addr->sa_family == AF_INET)
      {
        if (p->ifa_flags & IFF_BROADCAST)
        {
          struct sockaddr_in *si;

          si = (struct sockaddr_in *)p->ifa_broadaddr;
          sprintf(addr, "%s", inet_ntoa(si->sin_addr));

          break;
        }
      }
    }
  }

  freeifaddrs(ifa);
}

void parse_sockaddr_in(struct sockaddr_in *addr, const char *str)
{
  char tmp[256];
  char *a, *p;
  strcpy(tmp, str);
  a = tmp;
  p = strchr(tmp, ':');
  if(p) *(p++) = 0;
  addr->sin_family      = AF_INET;
  addr->sin_addr.s_addr = inet_addr(a);
  addr->sin_port        = p? htons(atoi(p)) : 0;
}

int broadcast_udp(void *buf, int len)
{
  static int sock = -1;
  static struct sockaddr_in br_addr;
  static pthread_mutex_t lk;
  int ret;
  if (sock<0)
  {
    int val;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      die("socket()");
    val = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
      die("setsockopt");
    br_addr.sin_family = AF_INET;
    br_addr.sin_port = htons(BRCAST_PORT);
    br_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    pthread_mutex_init(&lk, NULL);
  }

  ret = 0;
  /* need locking as we call it in multiple threads. */
  pthread_mutex_lock(&lk);
  if( sendto(sock, buf, len, 0, (struct sockaddr *)&br_addr, sizeof(br_addr)) < 0 )
  {
    perror("broadcast_udp");
    ret = -1;
  }
  pthread_mutex_unlock(&lk);
  return ret;
}

int send_udp(void *buf, size_t len, const struct sockaddr_in *addr)
{
  /* one udp sock for each process.
   * so cannot be called in two concurrent threads */
  static int sock = 0;

  if (!sock)
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      die("socket()");

  /* Send data to the server */
  if (sendto(sock, buf, len, 0,
    (struct sockaddr *)addr, sizeof(*addr)) < 0)
    fail("sendto()");

  /* not expecting any reply */

  return 0;
}

static int recv_all(int sock, void *buf, int size)
{
  int l, len = 0;
  int again = 0;
  while(1)
  {
    l = recv(sock, buf+len, size-len, 0);
    if (l > 0)
    {
      len += l;
      if( len >= size )
        return len;
    }
    else if(l==0)
      return len;
    else
    {
      if( errno==EAGAIN )
      {
        if( ++again > 600 )
          return -1;
        msleep(10);
        continue;
      }
      else
      {
        perror("recv()");
        return -1;
      }
    }
  }
}

/* connect to and send to the server.
 * the response is stored in buf. */
int send_tcp(void *buf, size_t len, const struct sockaddr_in *addr)
{
  int sock;
  unsigned char tmp[BUFLEN];
  int l;

  const int timeout = 6;

  struct timeval tv = {timeout, 0};
  fd_set fds;
  int so_err;
  socklen_t err_len = sizeof so_err;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
  setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

  /* make the connect() support timeout via select() */
  fcntl(sock, F_SETFL, O_NONBLOCK);

  if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
  {
    if(errno != EINPROGRESS)
    {
      perror("connect()");
      close(sock);
      return -2;
    }
  }
  trace_dbg("socket connected\n");

  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  /* select() writefds returns when connected. */
  if( select(sock+1, NULL, &fds, NULL, &tv) <= 0 )
  {
    perror("failed to connect with select()");
    close(sock);
    return -2;
  }
  trace_dbg("ready to send\n");

  if( getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_err, &err_len) < 0 || so_err != 0 )
  {
    perror("connect error");
    close(sock);
    return -2;
  }

  /* encode the data before sending out */
  l = encode(tmp, buf, len);

  if (send(sock, tmp, l, 0) < 0)
  {
    perror("send()");
    close(sock);
    return -2;
  }
  trace_dbg("sent cmd: %.80s\n", (char *)buf);

  /* recv any reply here */

  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if( select(sock+1, &fds, NULL, NULL, &tv) <= 0 )
  {
    perror("select() for recv()");
    close(sock);
    return -2;
  }

  while(1)
  {
    l=recv_all(sock, tmp, sizeof tmp);
    if (l > 0)
    {
      /* decode the reply and store in origin buf */
      l = decode(buf, tmp, l);
      ((char *)buf)[l]=0;
      trace_dbg("recved reply: %.80s\n", (char *)buf);
    }
    else if(l==0)
      fprintf(stderr, "(no repsponse)\n");
    else
    {
      perror("recv_all");
    }
    break;
  }

  close(sock);
  return l;
}

struct try_send_tcp_param_t
{
  char *buf;
  int len;
  struct sockaddr_in addr;
  void (*try_end)(char *);

  char bytes[1];
};

/* this thread runs until the command is successfully sent */
static void *try_send_tcp(void *arg)
{
  struct try_send_tcp_param_t *param = (struct try_send_tcp_param_t *)arg;

  while(1)
  {
    if (send_tcp(param->buf, param->len, &param->addr) > 0)
      break;
    else
      sleep(3);
  }

  param->try_end(param->buf);

  free(param);

  return NULL;
}

void start_try_send_tcp(void *buf, int len, const struct sockaddr_in *addr, void (*try_end)(char *))
{
  struct try_send_tcp_param_t *param =
    (struct try_send_tcp_param_t *)malloc(BUFLEN);
  memcpy(param->bytes, buf, len);
  param->buf = param->bytes;
  param->len = len;
  param->addr = *addr;
  param->try_end = try_end;

  start_thread(try_send_tcp, param);
}


static int audio_sock = -1;
static struct sockaddr_in audio_serv_addr;
static int dev_id;

static void repack_audio(const void *buf, int len)
{
#define PKSIZE  524
  const int pksize = PKSIZE;
  static char pack[1000];
  static int pklen = 0;
  static int pkrem = PKSIZE;
  int boff = 0;
  int l;

  while( len > 0 )
  {
    l = len<pkrem? len:pkrem;
    memcpy(pack+pklen, buf+boff, l);
    pklen += l;
    pkrem -= l;
    boff += l;
    len -= l;

    if( pklen == pksize )
    {
      /* deliter it */
      tcp_recved(audio_sock, /* audio type */ 2, pack, pklen);
      pklen = 0;
      pkrem = pksize;
    }
  }
}

static int __connect_audio_sock()
{
  int sock;
  char buf[20];
  uint32_t *pid;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (const struct sockaddr *)&audio_serv_addr, sizeof(audio_serv_addr)) < 0)
  {
    close(sock);
    fail("connect()");
  }

  /* send Hi right after the audio connection is
   * established, for the server to identify the device. */
  pid = (uint32_t *)buf;
  *pid = htonl(dev_id);
  buf[4] = 'H';
  buf[5] = 'i';
  if (send(sock, buf, 6, 0) < 0)
  {
    close(sock);
    fail("send Hi");
  }

  /* make the sock non-block */
  fcntl(sock, F_SETFL, O_NONBLOCK);

  audio_sock = sock;

  return 0;
}

static void __run_recv_audio()
{
  int l;
  char buf[BUFLEN];
  fd_set fds;
  int r;
  int s;

  /* audio packs are transmitted over a constent connection,
   * until server is closed. */

  while(1)
  {
    if( audio_sock < 0 )
    {
      sleep(3);
      __connect_audio_sock();
      continue;
    }

    /* recv until remote end closed. */
    for(;;)
    {
      FD_ZERO(&fds);
      FD_SET(audio_sock, &fds);

      r = select(audio_sock+1, &fds, NULL, NULL, NULL);
      if( r < 0 )
      {
        perror("recv audio: select()");
        break;
      }

      l = recv(audio_sock, buf, sizeof buf, 0);
      if( l <= 0 )
      {
        perror("recv audio: recv()");
        break;
      }
      else
      {
        repack_audio(buf, l);
      }
    }

    s = audio_sock;
    audio_sock = -1;
    close(s);
  }
}

static void *run_recv_audio(void *arg)
{
  __run_recv_audio();
  return NULL;
}

int connect_audio_sock(const struct sockaddr_in *addr, int did)
{
  int r;

  audio_serv_addr = *addr;
  dev_id = did;
  r = __connect_audio_sock();

  start_thread(run_recv_audio, NULL);

  return r;
}

int send_audio(void *buf, size_t len)
{
  int i;

  if( audio_sock < 0 )
  {
    return -1;
  }

  /* try 15 times. shutdown the sock if all failed
   * (remote end inactive). */
  for( i=0; i<15; i++ )
  {
    if (send(audio_sock, buf, len, 0) < 0)
    {
      perror("send audio");
      if( errno == EAGAIN )
        continue;
      else
        return -1;
    }
    else
      return 0;
  }

  /* this will unblock the select() in the recv thread,
   * thus allow it to try to reconnect. */
  shutdown(audio_sock, SHUT_RDWR);

  return -1;
}


static int udp_port;
static int udp_recv_flag;
/* the sock to recv multicast */
static int mc_sock = 0;

static void _recv_udp(int s);

static void *run_recv_udp(void *arg)
{
  int sock, br_sock=0;
  int on;
  int port;
  struct sockaddr_in addr; /* Local address */
  fd_set sks;
  int n;

  port = udp_port;

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  /* Eliminates "Address already in use" error from bind. */
  on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  /* Construct local address structure */
  memset(&addr, 0, sizeof(addr));   /* Zero out structure */
  addr.sin_family = AF_INET;                /* Internet address family */
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  addr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    die("bind()");

  printf("listen on %d\n", port);


  if( udp_recv_flag & F_UDP_RECV_BROADCAST )
  {
    /* Create socket for receving broadcasted packets */
    if ((br_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      die("socket()");

    on = 1;
    if (setsockopt(br_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
      die("setsockopt");

    on = 1;
    if (setsockopt(br_sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
      die("setsockopt");

    on = UDP_SOCK_BUFSIZE;
    if (setsockopt(br_sock, SOL_SOCKET, SO_RCVBUF, &on, sizeof(on)) < 0)
    {
      perror("setsockopt SO_RCVBUF on br_sock fail");
    }

    /* Bind to the broadcast port */
    addr.sin_port = htons(BRCAST_PORT);

    if (bind(br_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
      close(br_sock);
      br_sock = 0;
      perror("bind to broadcast port");
    }
  }

  if( udp_recv_flag & F_UDP_RECV_MULTICAST )
  {
    if ((mc_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      die("socket()");

    on = 1;
    if (setsockopt(mc_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
      die("setsockopt");

    on = UDP_SOCK_BUFSIZE;
    if (setsockopt(mc_sock, SOL_SOCKET, SO_RCVBUF, &on, sizeof(on)) < 0)
      perror("setsockopt SO_RCVBUF on br_sock fail");

    addr.sin_port = htons(MCAST_PORT);
    if (bind(mc_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
      close(mc_sock);
      mc_sock = 0;
      perror("bind to multicast port");
    }
  }

  /* n = max(sock, br_sock, mc_sock) + 1 */
  n = sock;
  if( n < br_sock )
    n = br_sock;
  if( n < mc_sock )
    n = mc_sock;
  n ++;

#define SET_SOCK(s)   \
  if (s>0)  \
    FD_SET(s, &sks)

#define CHECK_SOCK(s)  \
  if (s > 0 && FD_ISSET(s, &sks)) { \
    FD_CLR(s, &sks);  \
    _recv_udp(s); \
  }

  for (;;) /* Run forever */
  {
    FD_ZERO(&sks);
    SET_SOCK(sock);
    SET_SOCK(br_sock);
    SET_SOCK(mc_sock);

    if (select(n, &sks, NULL, NULL, NULL) > 0)
    {
      CHECK_SOCK(sock);
      CHECK_SOCK(br_sock);
      CHECK_SOCK(mc_sock);
    }
    else
    {
      perror("select");
      continue;
    }
  }
  /* NOT REACHED */
}

static void _recv_udp(int s)
{
  struct sockaddr_in other;
  size_t otherLen;
  char buf[BUFLEN];
  int len;

  otherLen = sizeof(other);

  if ((len = recvfrom(s, buf, sizeof buf, 0,
          (struct sockaddr *) &other, &otherLen)) < 0)
  {
    perror("recvfrom() failed");
    return;
  }

  //buf[len] = 0;
  //i = *(int *)buf;
  //p = buf+sizeof(int);
  //fprintf(stderr,"recved %d:%s\n", i, p);

  /* call the recv handler */
  if (udp_recved)
    udp_recved(buf, len);
}

int join_mcast_group(in_addr_t maddr)
{
  struct ip_mreq mreq;
  int r;
  if( mc_sock <= 0 )
    return -1;
  mreq.imr_multiaddr.s_addr = maddr;
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  r = setsockopt(mc_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));
  if( r<0 )
  {
    perror("setsockopt(IP_ADD_MEMBERSHIP)");
  }
  return r;
}

int quit_mcast_group(in_addr_t maddr)
{
  struct ip_mreq mreq;
  int r;
  if( mc_sock <= 0 )
    return -1;
  mreq.imr_multiaddr.s_addr = maddr;
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  r = setsockopt(mc_sock,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));
  if( r<0 )
  {
    perror("setsockopt(IP_DROP_MEMBERSHIP)");
  }
  return r;
}


static int tcp_port;
static int file_port;
static int tcp_recv_file;

static void *run_recv_tcp(void *arg)
{
  int port;
  int lisn_sock;
  int file_sock;
  int conn_sock;               /* Socket descriptor for incoming connection */
  fd_set sks;
  int n;
  int on;
  struct sockaddr_in loclAddr; /* local address */
  struct sockaddr_in remtAddr; /* remote address */
  size_t remtLen;
  char buf[BUFLEN];
  int l;

  port = tcp_port;

  if ((lisn_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

  if( tcp_recv_file )
  {
    if ((file_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      die("socket()");
  }
  else
    file_sock = -1;

  n = (lisn_sock>file_sock? lisn_sock:file_sock) +1;

  /* Eliminates "Address already in use" error from bind. */
  on = 1;
  if (setsockopt(lisn_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  /* Construct local address structure */
  memset(&loclAddr, 0, sizeof loclAddr);   /* Zero out structure */
  loclAddr.sin_family = AF_INET;                /* Internet address family */
  loclAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  loclAddr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(lisn_sock, (struct sockaddr *) &loclAddr, sizeof loclAddr) < 0)
    die("bind()");

  /* Mark the socket so it will listen for incoming connections */
  if (listen(lisn_sock, 50) < 0)
    die("listen()");

  if( file_sock > 0 )
  {
    on = 1;
    if (setsockopt(file_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
      die("setsockopt");

    /* cmd and file go separate ports */
    loclAddr.sin_port = htons(file_port);
    if (bind(file_sock, (struct sockaddr *) &loclAddr, sizeof loclAddr) < 0)
      die("bind()");

    if (listen(file_sock, 50) < 0)
      die("listen()");
  }

  for (;;) /* Run forever */
  {
    int sel = 0;
    int isfile = 0;

    /* Wait for a remote to connect */

    FD_ZERO(&sks);
    FD_SET(lisn_sock, &sks);
    if( file_sock > 0 )
      FD_SET(file_sock, &sks);

    if (select(n, &sks, NULL, NULL, NULL) > 0)
    {
      if (FD_ISSET(lisn_sock, &sks))
      {
        FD_CLR(lisn_sock, &sks);
        sel = lisn_sock;
        isfile = 0;
      }
      if (file_sock>0 && FD_ISSET(file_sock, &sks))
      {
        FD_CLR(file_sock, &sks);
        sel = file_sock;
        isfile = 1;
      }
    }
    else
    {
      perror("select");
      continue;
    }

    /* Set the size of the in-out parameter */
    remtLen = sizeof remtAddr;

    if ((conn_sock = accept(sel, (struct sockaddr *) &remtAddr, &remtLen)) < 0)
    {
      perror("accept()");
      continue;
    }

    /* conn_sock is connected to a remote! */
    if( !isfile )
      fprintf(stderr, "recved cmd: ");
    else
      fprintf(stderr, "recved file\n");

    if( (l = recv_all(conn_sock, buf, sizeof buf)) > 0 )
    {
      if( !isfile )
      {
        /* decode received data */
        unsigned char tmp[BUFLEN];
        l = decode(tmp, buf, l);
        memcpy(buf, tmp, l);
        buf[l] = 0;
        fprintf(stderr, "%s", buf);
      }

      /* call the recv handler */
      if (tcp_recved)
        tcp_recved(conn_sock, isfile, buf, l);

      /* send any here */
    }

    /* notify connection closing */
    if (tcp_recved)
      tcp_recved(0, isfile, NULL, 0);

    if( !isfile )
      fprintf(stderr, "\n");

    close(conn_sock);
  }
  /* NOT REACHED */
}

void start_recv_udp(int listenPort, void (*recved)(char *buf, int l), int flag)
{
  udp_port = listenPort;
  udp_recved = recved;
  udp_recv_flag = flag;
  start_thread(run_recv_udp, NULL);
}

void start_recv_tcp(int listenPort, void (*recved)(int sock, int isfile, char *buf, int l), int recv_file)
{
  tcp_port = listenPort;
  file_port = listenPort+1;
  tcp_recved = recved;
  tcp_recv_file = recv_file;
  start_thread(run_recv_tcp, NULL);
}

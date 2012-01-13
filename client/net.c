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

#define BUFLEN 4096

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

/* connect to and send to the server.
 * the response is stored in buf. */
int send_tcp(void *buf, size_t len, const struct sockaddr_in *addr)
{
  int sock;
  unsigned char tmp[BUFLEN];
  int l;

  const int timeout = 3;

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

  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  /* select() writefds returns when connected. */
  if( select(sock+1, NULL, &fds, NULL, &tv) < 0 )
  {
    perror("failed to connect with select()");
    close(sock);
    return -2;
  }

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

  /* recv any reply here */

  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if( select(sock+1, &fds, NULL, NULL, &tv) < 0 )
  {
    perror("select() for recv()");
    close(sock);
    return -2;
  }

  l=recv(sock, tmp, sizeof tmp, 0);
  if (l > 0)
  {
    /* decode the reply and store in origin buf */
    l = decode(buf, tmp, l);
  }
  else if(l==0)
    fprintf(stderr, "(no repsponse)\n");
  else
    perror("recv()");

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
    (struct try_send_tcp_param_t *)malloc(2048);
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

static void *run_recv_audio(void *arg __unused)
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
static int udp_recv_br;

static void _recv_udp(int s);

static void *run_recv_udp(void *arg __unused)
{
  int sock, br_sock;
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


  if( !udp_recv_br )
  {
    br_sock = 0;
  }
  else
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

    /* Bind to the broadcast port */
    addr.sin_port = htons(BRCAST_PORT);

    if (bind(br_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
      close(br_sock);
      br_sock = 0;
      perror("bind to broadcast port");
    }
  }

  n = (sock>br_sock? sock:br_sock) +1;

  for (;;) /* Run forever */
  {
    FD_ZERO(&sks);
    FD_SET(sock, &sks);
    if (br_sock)
      FD_SET(br_sock, &sks);

    if (select(n, &sks, NULL, NULL, NULL) > 0)
    {
      if (FD_ISSET(sock, &sks))
      {
        FD_CLR(sock, &sks);
        _recv_udp(sock);
      }
      if (br_sock > 0 && FD_ISSET(br_sock, &sks))
      {
        FD_CLR(br_sock, &sks);
        _recv_udp(br_sock);
      }
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



static int tcp_port;
static int file_port;

static void *run_recv_tcp(void *arg __unused)
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

  if ((file_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

  n = (lisn_sock>file_sock? lisn_sock:file_sock) +1;

  /* Eliminates "Address already in use" error from bind. */
  on = 1;
  if (setsockopt(lisn_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  if (setsockopt(file_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  /* Construct local address structure */
  memset(&loclAddr, 0, sizeof loclAddr);   /* Zero out structure */
  loclAddr.sin_family = AF_INET;                /* Internet address family */
  loclAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  loclAddr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(lisn_sock, (struct sockaddr *) &loclAddr, sizeof loclAddr) < 0)
    die("bind()");

  /* cmd and file go separate ports */
  loclAddr.sin_port = htons(file_port);
  if (bind(file_sock, (struct sockaddr *) &loclAddr, sizeof loclAddr) < 0)
    die("bind()");

  /* Mark the socket so it will listen for incoming connections */
  if (listen(lisn_sock, 50) < 0)
    die("listen()");

  if (listen(file_sock, 50) < 0)
    die("listen()");

  for (;;) /* Run forever */
  {
    int sel = 0;
    int isfile = 0;

    /* Wait for a remote to connect */

    FD_ZERO(&sks);
    FD_SET(lisn_sock, &sks);
    FD_SET(file_sock, &sks);

    if (select(n, &sks, NULL, NULL, NULL) > 0)
    {
      if (FD_ISSET(lisn_sock, &sks))
      {
        FD_CLR(lisn_sock, &sks);
        sel = lisn_sock;
        isfile = 0;
      }
      if (FD_ISSET(file_sock, &sks))
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

    while ((l = recv(conn_sock, buf, sizeof buf, 0)) > 0)
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

void start_recv_udp(int listenPort, void (*recved)(char *buf, int l), int recv_br)
{
  udp_port = listenPort;
  udp_recved = recved;
  udp_recv_br = recv_br;
  start_thread(run_recv_udp, NULL);
}

void start_recv_tcp(int listenPort, void (*recved)(int sock, int isfile, char *buf, int l))
{
  tcp_port = listenPort;
  file_port = listenPort+1;
  tcp_recved = recved;
  start_thread(run_recv_tcp, NULL);
}

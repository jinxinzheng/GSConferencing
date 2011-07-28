#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "util.h"
#include "include/encode.h"
#include "../config.h"

#define BUFLEN 4096

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

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
  {
    close(sock);
    fail("connect()");
  }

  /* encode the data before sending out */
  l = encode(tmp, buf, len);

  if (send(sock, tmp, l, 0) < 0)
  {
    close(sock);
    fail("send()");
  }

  /* recv any reply here */
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
  pthread_t thread;
  struct try_send_tcp_param_t *param =
    (struct try_send_tcp_param_t *)malloc(2048);
  memcpy(param->bytes, buf, len);
  param->buf = param->bytes;
  param->len = len;
  param->addr = *addr;
  param->try_end = try_end;

  pthread_create(&thread, NULL, try_send_tcp, param);
}


static int udp_port;
static void (*udp_recved)(char *buf, int l);

static void _recv_udp(int s);

static void *run_recv_udp(void *arg)
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
  char buf[BUFLEN], *p;
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
static void (*tcp_recved)(int sock, int isfile, char *buf, int l);

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

void start_recv_udp(int listenPort, void (*recved)(char *buf, int l))
{
  pthread_t thread;
  udp_port = listenPort;
  udp_recved = recved;
  pthread_create(&thread, NULL, run_recv_udp, NULL);
}

void start_recv_tcp(int listenPort, void (*recved)(int sock, int isfile, char *buf, int l))
{
  pthread_t thread;
  tcp_port = listenPort;
  file_port = listenPort+1;
  tcp_recved = recved;
  pthread_create(&thread, NULL, run_recv_tcp, NULL);
}

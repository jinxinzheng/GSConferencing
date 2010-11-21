#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "util.h"

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
  char tmp[2048];
  int l;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
    fail("connect()");

  if (send(sock, buf, len, 0) < 0)
    fail("send()");

  /* recv any reply here */
  l=recv(sock, tmp, sizeof tmp, 0);
  if (l > 0)
    memcpy(buf, tmp, l);
  else if(l==0)
    fprintf(stderr, "(no repsponse)\n");
  else
    perror("recv()");

  close(sock);
  return l;
}


static int udp_port;
static void (*udp_recved)(char *buf, int l);

static void *run_recv_udp(void *arg)
{
  int sock;
  int on;
  int port;
  struct sockaddr_in addr; /* Local address */
  struct sockaddr_in other;
  int otherLen;
  char buf[2048], *p;
  int len, i;

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

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    otherLen = sizeof(other);

    /* Block until receive message from a client */
    if ((len = recvfrom(sock, buf, 1024, 0,
            (struct sockaddr *) &other, &otherLen)) < 0)
    {
      perror("recvfrom() failed");
      continue;
    }

    buf[len] = 0;

    i = *(int *)buf;
    p = buf+sizeof(int);

    fprintf(stderr,"recved %d:%s\n", i, p);

    /* call the recv handler */
    if (udp_recved)
      udp_recved(buf, len);

  }
  /* NOT REACHED */

}

static int tcp_port;
static void (*tcp_recved)(char *buf, int l);

static void *run_recv_tcp(void *arg)
{
  int port;
  int lisn_sock;
  int conn_sock;               /* Socket descriptor for incoming connection */
  int on;
  struct sockaddr_in loclAddr; /* local address */
  struct sockaddr_in remtAddr; /* remote address */
  int remtLen;
  char buf[1024];
  int l;

  port = tcp_port;

  if ((lisn_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

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

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    remtLen = sizeof remtAddr;

    /* Wait for a remote to connect */
    if ((conn_sock = accept(lisn_sock, (struct sockaddr *) &remtAddr, &remtLen)) < 0)
      perror("accept()");

    /* conn_sock is connected to a remote! */

    while ((l = recv(conn_sock, buf, sizeof buf, 0)) > 0)
    {
      buf[l] = 0;
      fprintf(stderr, "recved cmd: %s\n", buf);

      /* call the recv handler */
      if (tcp_recved)
        tcp_recved(buf, l);

      /* send any here */
    }


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

void start_recv_tcp(int listenPort, void (*recved)(char *buf, int l))
{
  pthread_t thread;
  tcp_port = listenPort;
  tcp_recved = recved;
  pthread_create(&thread, NULL, run_recv_tcp, NULL);
}

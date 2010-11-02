#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "sys.h"
#include "devctl.h"
#include "cast.h"
#include "cmd.h"

#define die(s) do {perror(s); exit(1);} while(0)
#define fail(s) do {perror(s); return -1;} while(0)

struct connection
{
  int sock;
  struct sockaddr_in addr;

  struct list_head l;
};

static struct blocking_queue connection_queue;

static void enque_connection(struct connection *c)
{
  blocking_enque(&connection_queue, &c->l);
}

static struct connection *deque_connection()
{
  struct list_head *p;
  p = blocking_deque(&connection_queue);
  return list_entry(p, struct connection, l);
}

/* the global socket could be reused.
 * it should only be used within one thread. */
static int udp_sock;

#define MAXPENDING 50    /* Maximum outstanding connection requests */
#define BUFLEN 4096

void *run_proceed_connection(void *arg);

/* TCP listener loop
 * when an incoming connection/data is arrived the subsequent processing
 * should be done in a separate thread.
 * data packets are analyzed here, and dispatched to different services. */
void run_listener_tcp(int port)
{
  int tcp_sock;
  int clntSock;                /* Socket descriptor for client */
  struct sockaddr_in servAddr; /* Local address */
  struct sockaddr_in clntAddr; /* client address */
  socklen_t clntLen;
  int optval;
  struct connection *c;
  pthread_t connection_thread;

  blocking_queue_init(&connection_queue);

  if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

  /* Eliminates "Address already in use" error from bind. */
  optval = 1;
  if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    die("setsockopt");

  /* Construct local address structure */
  memset(&servAddr, 0, sizeof servAddr);   /* Zero out structure */
  servAddr.sin_family = AF_INET;                /* Internet address family */
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  servAddr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(tcp_sock, (struct sockaddr *) &servAddr, sizeof servAddr) < 0)
    die("bind()");

  /* Mark the socket so it will listen for incoming connections */
  if (listen(tcp_sock, MAXPENDING) < 0)
    die("listen()");

  /* the connected sockets proceeding thread */
  pthread_create(&connection_thread, NULL, run_proceed_connection, NULL);

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    clntLen = sizeof clntAddr;

    /* Wait for a client to connect */
    if ((clntSock = accept(tcp_sock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
      perror("accept()");

    /* clntSock is connected to a client! */

    /* put in the blocking queue*/
    c = (struct connection *)malloc(sizeof (struct connection));
    c->sock = clntSock;
    c->addr = clntAddr;
    enque_connection(c);

    /* immediately accept() again */
  }
  /* NOT REACHED */
}

void *run_proceed_connection(void *arg)
{
  struct connection *c;
  char buf[BUFLEN];
  int cmdl;
  int i;

  char *p;

  for (;;)
  {
    c = deque_connection();

    /* proceed with the connected socket */

    /* read until the remote closes the connection */
    while ((cmdl=recv(c->sock, buf, BUFLEN, 0)) > 0)
    {
      struct cmd cmd;
      char rep[BUFLEN];

      buf[cmdl]=0;
      printf("recved cmd: %s\n", buf);

      /* copy the command for reply use */
      strcpy(rep, buf);
      if (rep[cmdl-1] == '\n')
        rep[--cmdl] = 0;

      if (parse_cmd(buf, &cmd) != 0)
      {
        fprintf(stderr, "cmd parse error.\n");
        strcpy(rep, "FAIL parse error\n");
        goto CMDERR;
      }

      cmd.saddr = &c->addr;
      cmd.rep = rep;
      cmd.rl = cmdl;

      i = handle_cmd(&cmd);
      if (i < 0)
      {
        fprintf(stderr, "invalid cmd.\n");
        strcpy(rep, "FAIL invalid cmd\n");
        goto CMDERR;
      }
      else if (i > 0)
      {
        fprintf(stderr, "cmd handle error.\n");
        strcpy(rep, "FAIL handling cmd error\n");
        goto CMDERR;
      }

      /* send response */
      send(c->sock, cmd.rep, cmd.rl, 0);
      continue;

CMDERR:
      /* whatever error we must send the response
       * or the client hangs and so the server does!  */
      send(c->sock, rep, strlen(rep), 0);
    }

NEXT:
    close(c->sock);
    free(c);
  }

  return NULL;
}

void *listener_tcp_proc(void *p)
{
  int port;

  /* read port from config ? */
  port = 7650;

  run_listener_tcp(port);
}

void start_listener_tcp()
{
  pthread_t thread;

  pthread_create(&thread, NULL, listener_tcp_proc, NULL);
}


/* this should be running in main thread */
void run_listener_udp(int port)
{
  struct sockaddr_in servAddr; /* Local address */
  struct sockaddr_in clntAddr; /* Client address */
  unsigned int clntLen;         /* Length of incoming message */
  int optval;
  char buf[BUFLEN];        /* Buffer for echo string */
  int len;                 /* Size of received message */

  long did;

  /* Create socket for sending/receiving datagrams */
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  /* Eliminates "Address already in use" error from bind. */
  optval = 1;
  if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    die("setsockopt");

  /* Construct local address structure */
  memset(&servAddr, 0, sizeof servAddr);   /* Zero out structure */
  servAddr.sin_family = AF_INET;                /* Internet address family */
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  servAddr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(udp_sock, (struct sockaddr *) &servAddr, sizeof servAddr) < 0)
    die("bind()");

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    clntLen = sizeof(clntAddr);

    /* Block until receive message from a client */
    if ((len = recvfrom(udp_sock, buf, BUFLEN, 0, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
      perror("recvfrom()");

    /* analyze the packet data. */

    {
      struct device *d;

      /* work out the device object */
      did = *(long *)buf;
      d = get_device(did);
      if (!d)
        continue;

      /* dup and put data into processing queue */
      void *data = malloc(len+1);
      memcpy(data, buf, len);
      dev_cast_packet(d, 0, data, len);
    }

    /* Send any reply here */
  }
  /* NOT REACHED */
}

void listener_udp()
{
  int port;

  /* read port from config ? */
  port = 7650;

  run_listener_udp(port);
}

/* UDP send. not for reply use. */
int sendto_dev_udp(int sock, const void *buf, size_t len, struct device *dev)
{
  if (sendto(sock, buf, len, 0, (struct sockaddr *) &dev->addr, sizeof dev->addr) == -1)
    fail("sendto()");

  return 0;
}

/* TCP send. not for reply use. */
int sendto_dev_tcp(const void *buf, size_t len, struct device *dev)
{
  /* the tcp socket must be created for each call.
   * for the tcp socket could be connect()ed only once. */
  int sock;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (struct sockaddr *) &dev->addr, sizeof dev->addr) < 0)
    fail("connect()");

  if (send(sock, buf, len, 0) < 0)
    fail("send()");

  /* recv any reply here */

  close(sock);
  return 0;
}

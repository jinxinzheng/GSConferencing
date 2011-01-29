#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "include/queue.h"
#include "sys.h"
#include "devctl.h"
#include "cast.h"
#include "packet.h"
#include "cmd/cmd.h"
#include "include/debug.h"
#include "../config.h"

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
        sprintf(rep, "FAIL %d parse error\n", ERR_PARSE);
        goto CMDERR;
      }

      cmd.saddr = &c->addr;
      cmd.rep = rep;
      cmd.rl = cmdl;

      i = handle_cmd(&cmd);
      if (i < 0)
      {
        fprintf(stderr, "invalid cmd.\n");
        sprintf(rep, "FAIL %d invalid cmd\n", ERR_BAD_CMD);
        goto CMDERR;
      }
      else if (i > 0)
      {
        fprintf(stderr, "cmd handle error.\n");
        sprintf(rep, "FAIL %d handling cmd error\n", i);
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
  port = SERVER_PORT;

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

  struct packet *pack;

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


  pack = pack_get_new();

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    clntLen = sizeof(clntAddr);

    /* Block until receive message from a client */
    if ((pack->len = recvfrom(udp_sock, pack->data, sizeof pack->data, 0, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
    {
      perror("recvfrom()");
      continue;
    }

    /* analyze the packet data. */

    {
      struct device *d;
      struct group *g;

      /* work out the device object */
      did = (long)ntohl(*(uint32_t *)pack->data);
      d = get_device(did);
      if (!d)
        continue;

      g = d->group;

      /* don't cast if all are disabled by the chairman.
       * but we still need to cast the chairman's voice. */
      if( g->discuss.disabled && d != g->chairman )
        continue;

      /* don't cast if it is prohibited */
      if( d->discuss.forbidden || !d->discuss.open )
        continue;

      /* put packet into processing queue */
      pack->dev = d;
      if (dev_cast_packet(d, 0, pack) != 0)
        continue;
    }

    /* Send any reply here */

    pack = pack_get_new();
  }
  /* NOT REACHED */
}

void listener_udp()
{
  int port;

  /* read port from config ? */
  port = SERVER_PORT;

  run_listener_udp(port);
}

/* UDP send. not for reply use. */
int sendto_dev_udp(int sock, const void *buf, size_t len, struct device *dev)
{
  if (sendto(sock, buf, len, 0, (struct sockaddr *) &dev->addr, sizeof dev->addr) == -1)
    fail("sendto()");

  return 0;
}

int broadcast(struct tag *t, const void *buf, size_t len)
{
  int i;

  /* broadcast address for within the subnet */
  static struct sockaddr_in ba = {0};
  if (!ba.sin_family)
  {
    ba.sin_family = AF_INET;
    ba.sin_port = htons(BRCAST_PORT);
    ba.sin_addr.s_addr = inet_addr("255.255.255.255");
  }

  if (t->bcast_size == 0)
  {
    /* send to local network */
    if (sendto(t->sock, buf, len, 0, (struct sockaddr *) &ba, sizeof ba) == -1)
      fail(__func__);
  }
  else
  for (i=0; i<t->bcast_size; i++)
  {
    if (sendto(t->sock, buf, len, 0, (struct sockaddr *) t->bcasts[i], sizeof(struct sockaddr_in)) == -1)
      perror(__func__);
  }

  return 0;
}


#define _CONNECT_DEV(dev, sock) \
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) \
  { \
    perror("socket()"); \
    goto END; \
  } \
  if (connect(sock, (struct sockaddr *) &dev->addr, sizeof dev->addr) < 0) \
  { \
    perror("connect()"); \
    goto END; \
  }

#define _SEND(sock, buf, len) \
  if (send(sock, buf, len, 0) < 0) \
  { \
    perror("send()"); \
    goto END; \
  }

/* TCP send. not for reply use. */
void sendto_dev_tcp(const void *buf, size_t len, struct device *dev)
{
  /* the tcp socket must be created for each call.
   * for the tcp socket could be connect()ed only once. */
  int sock;

  fprintf(stderr, "sendto_dev_tcp( '%s', %d, %d )\n", (char*)buf, len, (int)dev->id);

  _CONNECT_DEV(dev, sock);

  _SEND(sock, buf, len);

  /* recv any reply here */

END:
  close(sock);
}

void send_file_to_dev(const char *path, struct device *dev)
{
  int sock;
  FILE *f;
  int fd;
  struct stat st;
  char buf[1024];
  int l;

  fprintf(stderr, "send_file_to_dev( '%s', %d )\n", path, (int)dev->id);

  if (!(f = fopen(path, "r")))
  {
    perror("fopen()");
    return;
  }

  fd = fileno(f);
  fstat(fd, &st);

  /* file transfer begins with a command:
   * "id file name length"
   * followed by the file content */

  l = sprintf(buf, "%d file %d\n", (int)dev->id, (int)st.st_size);

  _CONNECT_DEV(dev, sock);

  _SEND(sock, buf, l);

  /* ensure the client has done processing the command */
  close(sock);


  /* connect to file port */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket()");
    goto END;
  }
  if (connect(sock, (struct sockaddr *) &dev->fileaddr, sizeof dev->fileaddr) < 0)
  {
    perror("connect()");
    goto END;
  }

  /*
  while ((l=fread(buf, 1, sizeof buf, f)) > 0)
  {
    _SEND(sock, buf, l);
  } */
  sendfile(sock, fd, NULL, st.st_size);

  /* recv any reply here */

END:
  fclose(f);
  close(sock);
}

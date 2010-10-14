#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "sys.h"
#include "devctl.h"
#include "cast.h"

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
  int len;

  char *p;
  long did;
  char *cmd;

  for (;;)
  {
    c = deque_connection();

    /* proceed with the connected socket */
    if ((len = recv(c->sock, buf, BUFLEN, 0)) <= 0) {
      perror("recv()");
      goto NEXT;
    }

    /* analyze and process packet data here */
    buf[len] = 0;
    p = strtok(buf, " \r\n");
    did = atoi(p);
    cmd = strtok(NULL, " \r\n");

    if (!cmd) goto NEXT;

    if (strcmp(cmd, "reg") == 0)
    {
      struct device *newdev;

      p = strtok(NULL, " \r\n");
      if (p && p[0]=='p')
      {
        char *port = p+1;

        newdev = (struct device *)malloc(sizeof (struct device));
        newdev->id = did;
        newdev->addr = c->addr;
        newdev->addr.sin_port = htons(atoi(port));

        if (dev_register(newdev) != 0)
          free(newdev);
      }
    }
    else if (strcmp(cmd, "sub") == 0)
    {
      p = strtok(NULL, " \r\n");

      if (p) {
        struct device *d;
        struct tag *t;
        long tid, gid;
        long long tuid;

        d = get_device(did);

        gid = d->group->id;
        tid = atoi(p);
        tuid = TAGUID(gid, tid);
        t = get_tag(tuid);

        if (d) {
          if (!t)
            /* create an 'empty' tag that has no registered device.
             * this ensures the subscription not lost if there are
             * still not any device of the tag registered. */
            t = tag_create(gid, tid);

          dev_subscribe(d, t);
        }
      }
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

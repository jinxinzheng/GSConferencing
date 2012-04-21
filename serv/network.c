#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/sendfile.h>
#include "include/queue.h"
#include "sys.h"
#include "devctl.h"
#include "cast.h"
#include "packet.h"
#include "cmd/cmd.h"
#include "cmd_handler.h"
#include "include/pack.h"
#include "include/debug.h"
#include "include/encode.h"
#include "../config.h"
#include "hbeat.h"
#include "block.h"
#include "opts.h"
#include <include/ping.h>
#include <include/thread.h>
#include <include/util.h>
#include <include/compiler.h>

/* in cmd_video.c */
int cast_video(struct device *d, const void *buf, int len);

#define perrorf(fmt, args...) do{ \
  char __buf[256];  \
  sprintf(__buf, fmt, ##args); \
  perror(__buf);  \
} while(0)

struct connection
{
  int sock;
  struct sockaddr_in addr;

  struct list_head l;
};

static struct block_pool *conn_pool;

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
#define BUFLEN CMD_MAX

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

  if( sizeof(struct connection) > 64 )
  {
    fprintf(stderr, "connection size %d\n", (int)sizeof(struct connection));
    die("connection object size is too big.");
  }
  conn_pool = init_block_pool(64, 256, 1);

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
  start_thread(run_proceed_connection, NULL);

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    clntLen = sizeof clntAddr;

    /* Wait for a client to connect */
    if ((clntSock = accept(tcp_sock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
      perror("accept()");

    /* clntSock is connected to a client! */

    /* get connection object */
    while( !(c = (struct connection *)alloc_block(conn_pool)) )
    {
      sleep(1);
    }

    /* put in the blocking queue*/
    c->sock = clntSock;
    c->addr = clntAddr;
    enque_connection(c);

    /* immediately accept() again */
  }
  /* NOT REACHED */
}

static int do_recv(int sock, void *buf, int len)
{
  char tmp[BUFLEN];
  int l;

  if( (l = recv(sock, tmp, len, 0)) <= 0 )
    return l;

  l = decode(buf, tmp, l);

  return l;
}

static int do_send(int sock, const void *buf, int len)
{
  unsigned char tmp[BUFLEN];
  int l;

  l = encode(tmp, buf, len);

  return send(sock, tmp, l, 0);
}

void *run_proceed_connection(void *arg)
{
  struct connection *c;
  char buf[BUFLEN];
  int cmdl;
  int i;

  for (;;)
  {
    c = deque_connection();

    /* proceed with the connected socket */

    while( (cmdl=do_recv(c->sock, buf, BUFLEN)) > 0 )
    {
      struct cmd cmd;
      char rep[BUFLEN];

      buf[cmdl]=0;
      trace_info("recved cmd: %s\n", buf);

      /* copy the command for reply use */
      strcpy(rep, buf);
      if (rep[cmdl-1] == '\n')
        rep[--cmdl] = 0;

      if (parse_cmd(buf, &cmd) != 0)
      {
        trace_err("cmd parse error.\n");
        sprintf(rep, "FAIL %d parse error\n", ERR_PARSE);
        goto CMDERR;
      }

      cmd.saddr = &c->addr;
      cmd.rep = rep;
      cmd.rl = cmdl;
      cmd.sock = c->sock;

      i = handle_cmd(&cmd);
      if (i < 0)
      {
        trace_err("invalid cmd.\n");
        sprintf(rep, "FAIL %d invalid cmd\n", ERR_BAD_CMD);
        goto CMDERR;
      }
      else if (i > 0)
      {
        trace_err("cmd handle error.\n");
        sprintf(rep, "FAIL %d handling cmd error\n", i);
        goto CMDERR;
      }

      /* send response */
      if( cmd.rl > 0 )
        do_send(c->sock, cmd.rep, cmd.rl);

      /* the handler can choose to 'persist' the connection:
       * the socket will be used in a thread created by the
       * handler, so do not close it. */
      if( cmd.sock == 0 )
      {
        c->sock = 0;
      }

      /* read only 1 cmd for a connection.
       * this is useful for 'big' replies:
       * the client could recv until we close. */
      break;

CMDERR:
      /* whatever error we must send the response
       * or the client hangs and so the server does!  */
      do_send(c->sock, rep, strlen(rep));
      break;
    }

    if( c->sock )
      close(c->sock);
    free_block(conn_pool, c);
  }

  return NULL;
}

void *listener_tcp_proc(void *arg)
{
  int port;

  /* read port from config ? */
  port = SERVER_PORT;

  run_listener_tcp(port);

  return NULL;
}


int dev_send_audio(struct device *d, const void *buf, int len)
{
  /* this returns immediately on non-block socket.
   * try only once as it works generally on good connection. */
  if( send(d->audio_sock, buf, len, 0) < 0 )
  {
    perrorf("%d send audio fail", d->id);
    if( errno == EAGAIN )
    {
      if( ++d->audio_bad > 30 )
      {
        /* send() fails EAGAIN when, e.g, the network wire
         * is unplugged without any prompt. but the recv thread
         * cannot detect any error as the connection is still
         * in established state. */
        fprintf(stderr, "%d remote is inactive, shutdown.\n", d->id);
        shutdown(d->audio_sock, SHUT_RDWR);
        d->audio_bad = 0;
      }
    }
    return -1;
  }

  d->audio_bad = 0;

  return 0;
}

static int pack_recv(struct packet *pack);

static void *run_recv_audio(void *arg)
{
  struct device *d = (struct device *)arg;
  int conn_sock = d->audio_sock;
  char buf[BUFLEN];
  int len;

  struct packet *pack;

  const int pksize = 524;
  int pklen = 0;
  int pkrem = pksize;
  int boff = 0;
  int l;

  fd_set fds;

  pack = pack_get_new();

  while(1)
  {
    FD_ZERO(&fds);
    FD_SET(conn_sock, &fds);

    if( select(conn_sock+1, &fds, NULL, NULL, NULL) < 0 )
    {
      /* any error should not happen if the connection is good. */
      perror("recv_audio: select()");
      break;
    }

    if( (len = recv(conn_sock, buf, sizeof buf, 0)) <= 0 )
    {
      /* recv() should succeed since approved by select(). */
      perror("recv_audio: recv()");
      break;
    }

    /* repack the data into fixed-size chunks */

    boff = 0;
    while( len > 0 )
    {
      l = len<pkrem? len:pkrem;
      memcpy(pack->data+pklen, buf+boff, l);
      pklen += l;
      pkrem -= l;
      boff += l;
      len -= l;

      if( pklen == pksize )
      {
        pack->len = pklen;
        if( !pack_recv(pack) )
        {
          pack = pack_get_new();
        }

        pklen = 0;
        pkrem = pksize;
      }
    }
  }

  /* the device is disconnected,
   * so we invalidate the sock. */
  d->audio_sock = -1;

  close(conn_sock);

  trace_info("audio of dev %d disconnected\n", d->id);

  return NULL;
}

static void audio_connect(int sock)
{
  char buf[100];
  int l;

  /* expect the first Hi from the device. */
  l = recv(sock, buf, sizeof buf, 0);
  if( l >= 6 )
  {
    /* check the 'magic' */
    if( buf[4] == 'H' && buf[5] == 'i' )
    {
      int did;
      struct device *d;
      uint32_t *pid;

      pid = (uint32_t *)buf;
      did = ntohl(*pid);
      if( (d = get_device(did)) )
      {
        /* make the sock non-block */
        fcntl(sock, F_SETFL, O_NONBLOCK);

        d->audio_sock = sock;
        trace_info("audio connected to dev %d\n", did);

        /* fork a thread to receive the audio packs. */
        start_thread(run_recv_audio, (void *)d);
        return;
      }
    }
  }

  close(sock);
}

static int __run_listen_audio()
{
  int audio_sock;
  int conn_sock;
  int on;
  struct sockaddr_in loclAddr;

  int audio_port = SERVER_PORT + 2;

  /* audio packs are transmitted over a constent connection
   * associated with each device. */

  if ((audio_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  on = 1;
  if (setsockopt(audio_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    perror("setsockopt");

  memset(&loclAddr, 0, sizeof loclAddr);
  loclAddr.sin_family = AF_INET;
  loclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  loclAddr.sin_port = htons(audio_port);

  if (bind(audio_sock, (struct sockaddr *) &loclAddr, sizeof loclAddr) < 0)
    fail("bind()");

  if (listen(audio_sock, 50) < 0)
    fail("listen()");

  for(;;)
  {
    struct sockaddr_in remtAddr;
    socklen_t remtLen;

    /* Set the size of the in-out parameter */
    remtLen = sizeof remtAddr;

    if ((conn_sock = accept(audio_sock, (struct sockaddr *) &remtAddr, &remtLen)) < 0)
    {
      perror("accept()");
      continue;
    }

    trace_info("audio connect from %s\n", inet_ntoa(remtAddr.sin_addr));

    audio_connect(conn_sock);
  }

  return 0;
}

static void *run_listen_audio(void *arg)
{
  __run_listen_audio();
  return NULL;
}

void start_listener_tcp()
{
  start_thread(listener_tcp_proc, NULL);
  if( opt_tcp_audio )
    start_thread(run_listen_audio, NULL);
}


/* return: 1 if pack can be freed, 0 otherwise. */
static int pack_recv(struct packet *pack)
{
  pack_data *p = (pack_data *)pack->data;
  struct device *d;
  struct group *g;
  int i;
  uint32_t did;

  /* work out the device object */
  did = ntohl(p->id);
  d = get_device(did);
  if (!d)
    return 1;

  i = p->type;
  switch ( i )
  {
    case PACKET_HBEAT :

      dev_heartbeat(d);
      return 1;


    case PACKET_AUDIO :
    case PACKET_AUDIO_ZERO :

      g = d->group;

      /* don't cast if all are disabled by the chairman.
       * but we still need to cast the chairman's voice. */
      if( g->discuss.disabled && d != g->chairman )
        return 1;

      /* don't cast if it is prohibited */
      if( d->discuss.forbidden || !d->discuss.open )
        return 1;

      /* put packet into processing queue */
      if (dev_cast_packet(d, pack) != 0)
        return 1;

      break;

    case PACKET_REPEAT_REQ :
    {
      struct tag *t;
      g = d->group;
      t = get_tag(TAGUID(g->id, p->tag));
      if(t)
        //tag_repeat_cast(t, ntohl(p->seq));
        tag_req_repeat(t, d, ntohl(p->seq));
      return 1;
    }

    case PACKET_VIDEO:
      cast_video(d, pack->data, pack->len);
      return 1;
  }

  return 0;
}

/* this should be running in main thread */
void run_listener_udp(int port)
{
  struct sockaddr_in servAddr; /* Local address */
  struct sockaddr_in clntAddr; /* Client address */
  unsigned int clntLen;         /* Length of incoming message */
  int optval;

  struct packet *pack;

  /* Create socket for sending/receiving datagrams */
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  /* Eliminates "Address already in use" error from bind. */
  optval = 1;
  if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    die("setsockopt");

  optval = UDP_SOCK_BUFSIZE;
  if (setsockopt(udp_sock, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0)
    perror("udp sock setsockopt SO_RCVBUF");

  /* Construct local address structure */
  memset(&servAddr, 0, sizeof servAddr);   /* Zero out structure */
  servAddr.sin_family = AF_INET;                /* Internet address family */
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  servAddr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(udp_sock, (struct sockaddr *) &servAddr, sizeof servAddr) < 0)
    die("bind()");


  pack = pack_get_fast();

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

    if( pack->len < offsetof(pack_data, data) )
    {
      trace_warn("malformed pack len=%d\n", pack->len);
      continue;
    }

    /* analyze the packet data. */
    if( pack_recv(pack) )
      continue;

    /* Send any reply here */

    pack = pack_get_fast();
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

int broadcast_local(int sock, const void *buf, size_t len)
{
  /* broadcast address for within the local subnet */
  static struct sockaddr_in ba = {
    .sin_family = 0,
  };

  if( !ba.sin_family )
  {
    ba.sin_family = AF_INET;
    ba.sin_port = htons(BRCAST_PORT);
    ba.sin_addr.s_addr = inet_addr("255.255.255.255");
  }

  if( sendto(sock, buf, len, 0, (struct sockaddr *) &ba, sizeof ba) == -1 )
    fail(__func__);

  return 0;
}

int broadcast(struct tag *t, const void *buf, size_t len)
{
  int i;

  if (t->bcast_size == 0)
  {
    return broadcast_local(t->sock, buf, len);
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
  if (do_send(sock, buf, len) < 0) \
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

#ifdef DEBUG_INFO
  ((char*)buf)[len] = 0;
  trace_info("%s( '%s', %d, %d )\n", __func__, (char*)buf, (int)len, (int)dev->id);
#endif

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

  trace_info("%s( '%s', %d )\n", __func__, path, (int)dev->id);

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

int ping_dev(struct device *dev)
{
  return ping(&dev->addr);
}

int open_broadcast_sock()
{
  int sock;
  int val;

  if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
  {
    perror("socket()");
    return -1;
  }

  val = 1;
  if( setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof val) < 0 )
  {
    perror("setsockopt(SO_BROADCAST)");
    close(sock);
    return -1;
  }

  val = UDP_SOCK_BUFSIZE;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val)) < 0)
    perror("setsockopt(SO_SNDBUF)");

  return sock;
}

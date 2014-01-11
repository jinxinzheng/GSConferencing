#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <thread.h>
#include <pthread.h>
#include <util.h>
#include <lock.h>
#include "com.h"
#include "opt.h"
#include "log.h"

int open_udp_sock(int port)
{
  int sock;
  struct sockaddr_in addr;
  int on;
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  if( port == 0 )
  {
    return sock;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    die("bind()");
  
  return sock;
}

int open_tcp_sock(int port)
{
  int sock;
  struct sockaddr_in addr;
  int on;
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

  on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on)) < 0)
    die("setsockopt");

  if( port == 0 )
  {
    return sock;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    die("bind()");

  return sock;
}

void listen_sock(int sock)
{
  listen(sock, 50);
}

int accept_sock(int sock)
{
  int conn_sock;
  struct sockaddr_in remtAddr;
  size_t remtLen;
  if( (conn_sock = accept(sock, (struct sockaddr *) &remtAddr, &remtLen)) < 0 )
  {
    perror("accept()");
    return -1;
  }
  LOG("tcp connect from %s:%d\n", inet_ntoa(remtAddr.sin_addr), ntohs(remtAddr.sin_port));
  return conn_sock;
}

int push_sock;
int connected;
static struct sockaddr_in push_addr;
static pthread_mutex_t send_lk = PTHREAD_MUTEX_INITIALIZER;

void set_push_addr(const char *addr, int port)
{
  memset(&push_addr, 0, sizeof(push_addr));
  push_addr.sin_family = AF_INET;
  push_addr.sin_addr.s_addr = inet_addr(addr);
  push_addr.sin_port = htons(port);
}

static int recv_data_size(int sock, char *buf, int size)
{
  int len = 0;
  while( len < size )
  {
    int l = recv(sock, buf+len, size-len, 0);
    if( l == 0 )
    {
      LOG("connection shutdown\n");
      return -1;
    }
    if( l < 0 )
    {
      perror("recv()");
      LOG("connection closed error\n");
      return -1;
    }
    len += l;
  }
  return len;
}

int recv_data(int sock, char *buf, int size)
{
  int len;
  static struct sockaddr_in src_addr;
  int addrlen = sizeof(src_addr);
  if( tcp )
  {
    struct com_pack *p;
    p = (struct com_pack *) buf;
    len = recv_data_size(sock, buf, HEAD_SIZE);
    if( len <= 0 )
      return -1;
    len += recv_data_size(sock, buf+HEAD_SIZE, p->len);
    if( len <= 0 )
      return -1;
    LOG("received data %d\n", len);
    return len;
  }
  else
  {
    len = recvfrom(sock, buf, size, 0,
        (struct sockaddr *)&src_addr, &addrlen);
    LOG("data %d from remote %s:%d\n", len, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
    // if the remote has first connected to us or the remote address changed.
    if( push_addr.sin_addr.s_addr != src_addr.sin_addr.s_addr ||
        push_addr.sin_port != src_addr.sin_port )
    {
      push_addr = src_addr;
    }
    return len;
  }
}

int send_data(void *buf, int len)
{
  if( !push_sock )
    return 0;
  if( tcp && !connected )
    return 0;
  LOCK(send_lk);
  if( tcp )
  {
    send(push_sock, buf, len, 0);
  }
  else
  {
    if( push_addr.sin_port != 0 )
    {
      sendto(push_sock, buf, len, 0,
          (struct sockaddr *)&push_addr, sizeof(push_addr));
    }
  }
  UNLOCK(send_lk);
  return 1;
}

int send_pack(struct com_pack *p)
{
  return send_data(p, HEAD_SIZE+p->len);
}

int connect_push()
{
  struct com_pack p = {
    .ver = 0,
    .cmd = CMD_CONNECT,
    .len = 0,
  };
  if( tcp )
  {
    if( connect(push_sock, (struct sockaddr *)&push_addr, sizeof(push_addr)) < 0 )
    {
      perror("connect()");
      return 0;
    }
    connected = 1;
    LOG("connected\n");
  }
  send_pack(&p);
  return 1;
}

int send_audio(int tag, int type, void *data, int len)
{
  static uint16_t seq;
  char buf[4096];
  struct com_pack *pp = (struct com_pack *)buf;
  LOG("send audio %d of %d\n", len, tag);
  pp->ver = 0;
  pp->cmd = CMD_AUDIO;
  pp->u.audio.tag = tag;
  pp->u.audio.type = type;
  pp->u.audio.seq = seq++;
  memcpy(pp->u.audio.data, data, len);
  pp->len = len + ((char *)&pp->u.audio.data - (char *)&pp->u.audio);
  send_pack(pp);
  return 1;
}

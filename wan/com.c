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

int push_sock;
static struct sockaddr_in push_addr;
static pthread_mutex_t send_lk = PTHREAD_MUTEX_INITIALIZER;

void set_push_addr(const char *addr, int port)
{
  memset(&push_addr, 0, sizeof(push_addr));
  push_addr.sin_family = AF_INET;
  push_addr.sin_addr.s_addr = inet_addr(addr);
  push_addr.sin_port = htons(port);
}

int recv_data(int sock, void *buf, int size)
{
  int len;
  static struct sockaddr_in src_addr;
  int addrlen = sizeof(src_addr);
  if( push_sock != sock )
  {
    push_sock = sock;
  }
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

int send_data(void *buf, int len)
{
  if( !push_sock || !push_addr.sin_port )
  {
    return 0;
  }
  LOCK(send_lk);
  sendto(push_sock, buf, len, 0,
      (struct sockaddr *)&push_addr, sizeof(push_addr));
  UNLOCK(send_lk);
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

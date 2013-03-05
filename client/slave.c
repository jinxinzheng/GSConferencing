#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
#include  <arpa/inet.h>
#include  <include/util.h>
#include  <include/pack.h>
#include  "master.h"
#include  "../config.h"

static int sock;

static int init_sock()
{
  int s, val;
  struct sockaddr_in addr;

  /* Create socket for receving broadcasted packets */
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  val = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    die("setsockopt");

  val = 1;
  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
    die("setsockopt");

  /* Bind to the broadcast port */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(MASTER_HEART_PORT);

  if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0)
  {
    close(s);
    die("bind");
  }

  return s;
}

int wait_master_heart()
{
  struct sockaddr_in other;
  size_t other_len;
  char buf[2048];
  int len;
  struct pack *p;

  if( sock==0 )
  {
    sock = init_sock();
  }

  other_len = sizeof(other);

  if( (len = recvfrom(sock, buf, sizeof(buf), 0,
          (struct sockaddr *) &other, &other_len)) < 0 )
  {
    perror("recvfrom");
    return -1;
  }

  /* sanity checks */
  if( len != PACK_HEAD_LEN )
    return -1;

  p = (struct pack *)buf;
  if( p->id != MASTER_ID )
    return -1;

  if( p->type != PACKET_MASTER_HEART )
    return -1;

  return p->seq;
}

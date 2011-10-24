#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <include/pack.h>
#include <include/thread.h>
#include <include/util.h>
#include "network.h"
#include "../config.h"

static void *run_enounce(void *arg)
{
  int sock;
  int r, optval;
  pack_data pack;
  int len;

  if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    return NULL;

  optval = 1;
  if( (r = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval)) < 0 )
  {
    perror("setsockopt()");
    close(sock);
    return NULL;
  }

  memset(&pack, 0, sizeof pack);
  pack.id = htonl(0xE2012CE); /* the magic */
  pack.type = htons(PACKET_ENOUNCE);
  pack.tag = htons(SERVER_PORT);  /* use tag to store the port */
  len = offsetof(pack_data, data);

  for(;;sleep(5))
  {
    broadcast_local(sock, &pack, len);
  }

  return NULL;
}

void start_server_enounce()
{
  start_thread(run_enounce, NULL);
}

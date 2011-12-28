#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <include/pack.h>
#include <include/thread.h>
#include <include/util.h>
#include <include/compiler.h>
#include "network.h"
#include "../config.h"

static void *run_enounce(void *arg __unused)
{
  int sock;
  pack_data pack;
  int len;

  if( (sock = open_broadcast_sock()) < 0 )
    return NULL;

  memset(&pack, 0, sizeof pack);
  pack.id = 0xE2012CE; /* the magic */
  pack.type = PACKET_ENOUNCE;
  pack.seq = SERVER_PORT;  /* use seq to store the port */
  P_HTON(&pack);

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

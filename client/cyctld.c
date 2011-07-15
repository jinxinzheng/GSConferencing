#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <include/ping.h>

#include "cyctl.h"

static char *server_addr;
static int is_cyctl;

static void run_cyctl()
{
  int fail = 0;
  struct sockaddr_in servAddr;

  if( is_cyctl )
  {
    do_cyctl(0);
  }

  servAddr.sin_family      = AF_INET;
  servAddr.sin_addr.s_addr = inet_addr(server_addr);

  while( 1 )
  {
    sleep(10);

    if( is_cyctl )
    {
      if( ping(&servAddr) )
      {
        fail = 0;
      }
      else
      {
        if( ++fail >= 3 )
        {
          printf("cyclic rescue because ping server failed.\n");
          do_cyctl(1);
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  server_addr = argv[1];
  is_cyctl = atoi(argv[2]);

  if( !is_cyctl )
  {
    return 0;
  }

  run_cyctl();

  return 0;
}

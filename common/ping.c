#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

int ping(const struct sockaddr_in *addr)
{
  char cmd[256];
  int x;

  sprintf(cmd, "ping -c3 %s >/dev/null", inet_ntoa(addr->sin_addr));

  x = system(cmd);

  return x==0;
}

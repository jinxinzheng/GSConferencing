#include "network.h"

int main(int argc, char *const argv[])
{

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();
}

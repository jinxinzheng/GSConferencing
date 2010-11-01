#include "sys.h"
#include "cmd.h"
#include "network.h"

void init()
{
  init_groups();

  init_cmd_handlers();
}

int main(int argc, char *const argv[])
{
  init();

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();
}

#include "sys.h"
#include "cmd_handler.h"
#include "network.h"
#include "db/md.h"

void init()
{
  init_groups();

  init_cmd_handlers();

  md_load_all();
}

int main(int argc, char *const argv[])
{
  init();

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();
}

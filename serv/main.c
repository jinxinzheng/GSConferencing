#include "sys.h"
#include "cmd_handler.h"
#include "hbeat.h"
#include "network.h"
#include "db/md.h"
#include <unistd.h>
#include "opts.h"
#include "recover.h"

void init()
{
  init_groups();

  init_cmd_handlers();

  md_load_all();
}

int main(int argc, char *const argv[])
{
  int opt;

  while ((opt = getopt(argc, argv, "q:B")) != -1) {
    switch (opt) {
      case 'q':
        opt_queue_max = atoi(optarg);
        break;
      case 'B':
        opt_broadcast = 0;
        break;
      default:
        break;
    }
  }

  init();

  recover_server();

  start_heartbeat_god();

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();

  return 0;
}

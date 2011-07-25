#include "sys.h"
#include "init.h"
#include "cmd_handler.h"
#include "hbeat.h"
#include "network.h"
#include "packet.h"
#include "db/md.h"
#include <unistd.h>
#include "opts.h"
#include "recover.h"
#include "async.h"
#include <config.h>

static void basic_setup()
{
  init_groups();

  init_cmd_handlers();

  init_pack_pool();

  async_init();

  md_load_all();
}

int main(int argc, char *const argv[])
{
  int opt;

  printf("daya server %s\n", VERSION);

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

  if( init() != 0 )
    return 1;

  basic_setup();

  recover_server();

  start_heartbeat_god();

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();

  return 0;
}

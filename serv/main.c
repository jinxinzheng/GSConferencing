#include "sys.h"
#include "cmd_handler.h"
#include "network.h"
#include "db/md.h"
#include <unistd.h>
#include "opts.h"

void init()
{
  init_groups();

  init_cmd_handlers();

  md_load_all();
}

int main(int argc, char *const argv[])
{
  int opt;

  while ((opt = getopt(argc, argv, "q:")) != -1) {
    switch (opt) {
      case 'q':
        opt_queue_max = atoi(optarg);
        break;
      default:
        break;
    }
  }

  init();

  start_listener_tcp();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();
}

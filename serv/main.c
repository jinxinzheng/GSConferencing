#include "sys.h"
#include "init.h"
#include "cmd_handler.h"
#include "hbeat.h"
#include "enounce.h"
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

static void help()
{
  printf(
      "options:\n"
      "-h:        print this help.\n"
      "-B:        don't use broadcast.\n"
      "-u:        always use udp to cast audio.\n"
      "-f:        enable packet flush dropping.\n"
      "-s:        enable packet silence dropping.\n"
      "-m MIX:    set mix method: simple, net(default).\n"
      "-p POLICY: (for only -m simple) set syncing policy: wait(default), fixed, ref.\n"
  );
}

int main(int argc, char *const argv[])
{
  int opt;

  if( init() != 0 )
    return 1;

  printf("daya server %s\n", VERSION);

  while ((opt = getopt(argc, argv, "hq:Butfp:sm:")) != -1) {
    switch (opt) {
      case 'h':
        help();
        return 0;
        break;
      case 'q':
        opt_queue_max = atoi(optarg);
        break;
      case 'B':
        opt_broadcast = 0;
        break;
      case 'u':
        opt_tcp_audio = 0;
        break;
      case 't':
        opt_tcp_audio = 1;
        break;
      case 'f':
        opt_flush = 1;
        break;
      case 'p':
        if( strcmp("wait", optarg) == 0 )
          opt_sync_policy = SYNC_WAIT;
        else if( strcmp("fixed", optarg) == 0 )
          opt_sync_policy = SYNC_FIXED;
        else if( strcmp("ref", optarg) == 0 )
          opt_sync_policy = SYNC_REF;
        else
        {
          fprintf(stderr, "command parse error.\n");
          return 1;
        }
        break;
      case 's':
        opt_silence_drop = 1;
        break;
      case 'm':
        if( strcmp("simple", optarg) == 0 )
          opt_mixer = MIXER_SIMPLE;
        else if( strcmp("net", optarg) == 0 )
          opt_mixer = MIXER_NET;
        else
        {
          fprintf(stderr, "invalid command arg.\n");
          help();
          return 1;
        }
        break;
      case '?':
        help();
        return 1;
        break;
    }
  }

  basic_setup();

  recover_server();

  start_heartbeat_god();

  start_listener_tcp();

  start_server_enounce();

  /* this will execute on the main thread and keep the process alive. */
  listener_udp();

  return 0;
}

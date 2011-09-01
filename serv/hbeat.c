#include "sys.h"
#include "devctl.h"
#include "db/md.h"
#include <stdio.h>
#include <unistd.h>
#include "network.h"
#include "include/debug.h"

void dev_heartbeat(struct device *d)
{
  d->hbeat = 0;

  if( !d->active )
  {
    struct db_device *dbd;
    if( (dbd = d->db_data) )
    {
      dbd->online = 1;
      device_save(d);
    }
    d->active = 1;
  }
}

static void check_net(struct device *d)
{
  if( !ping_dev(d) )
  {
    struct group *g = d->group;
    char cmd[256];
    int l;

    /* activate emergency handler */
    if( g->cyctl )
    {
      trace_warn("activating cyclic rescue\n");
      l = sprintf(cmd, "0 cyc_ctl %d\n", (int)d->id);
      device_cmd(g->cyctl, cmd, l);
    }
  }
}

static void *run_heartbeat_god(void *arg)
{
  struct group *g;
  struct device *d;

  while( 1 )
  {
    sleep(10);

    if( !(g = get_group(1)) )
      continue;

    list_for_each_entry(d, &g->device_head, list)
    {
      if( ++ d->hbeat > 3 )
      {
        /* 1 minute has been hit */
        if( d->active )
        {
          struct db_device *dbd;
          if( (dbd = d->db_data) )
          {
            dbd->online = 0;
            device_save(d);
          }
          d->active = 0;
          trace_warn("dev %d is dead\n", (int)d->id);

          check_net(d);
        }
      }
    }
  }

  /* never reached */
  return 0;
}

void start_heartbeat_god()
{
  pthread_t god;
  pthread_create(&god, NULL, run_heartbeat_god, NULL);
}

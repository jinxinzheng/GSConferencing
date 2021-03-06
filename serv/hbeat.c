#include "sys.h"
#include "devctl.h"
#include "db/md.h"
#include <stdio.h>
#include <unistd.h>
#include "network.h"
#include "discuss.h"
#include "include/debug.h"
#include <include/thread.h>
#include <include/compiler.h>
#include <arpa/inet.h>

void heartbeat_id(int did)
{
  struct db_device *dd;
  dd = md_find_device(did);
  if( dd )
  {
    dd->online = 1;
  }
}

void dev_heartbeat(struct device *d)
{
  d->hbeat = 0;

  if( !d->active )
  {
    dev_activate(d);
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
        /* time-out has been hit */
        if( d->active )
        {
          dev_deactivate(d);
          /* force it release the resources */
          if( d->discuss.open )
            del_open(d->tag, d);
          trace_warn("dev %d (%s) is dead\n", d->id, inet_ntoa(d->addr.sin_addr));

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
  start_thread(run_heartbeat_god, NULL);
}

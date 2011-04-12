#include "sys.h"
#include "db/md.h"
#include <stdio.h>

void dev_heartbeat(struct device *d)
{
  d->hbeat = 0;

  if( !d->active )
  {
    struct db_device *dbd;
    if( dbd = d->db_data )
    {
      dbd->online = 1;
      device_save(d);
    }
    d->active = 1;
  }
}

static void *run_heartbeat_god(void *arg)
{
  struct group *g;
  struct device *d;
  struct list_head *p;

  while( 1 )
  {
    sleep(10);

    if( !(g = get_group(1)) )
      continue;

    list_for_each(p, &g->device_head)
    {
      d = list_entry(p, struct device, list);
      if( ++ d->hbeat > 6 )
      {
        /* 1 minute has been hit */
        if( d->active )
        {
          struct db_device *dbd;
          if( dbd = d->db_data )
          {
            dbd->online = 0;
            device_save(d);
          }
          d->active = 0;
          fprintf(stderr, "dev %d is dead\n", (int)d->id);
        }
      }
    }
  }
}

void start_heartbeat_god()
{
  pthread_t god;
  pthread_create(&god, NULL, run_heartbeat_god, NULL);
}

#include  "mix.h"
#include  <unistd.h>
#include  <include/util.h>
#include  <include/compiler.h>

/* the whole mixing process is handled to the
 * hardware net mixer device. so we only need to
 * implement stub functions for put() and get().
 * but we need to allocate the resource for each
 * dev in open() and close(). */

struct net_resource {
  int avail;
  const char *addr;
};
/* TODO: program the hard-coded array.. */
static struct net_resource resources[] = {
  {1, "192.168.1.11"},
  {1, "192.168.1.12"},
  {1, "192.168.1.13"},
  {1, "192.168.1.14"},
  {1, "192.168.1.15"},
  {1, "192.168.1.16"},
  {1, "192.168.1.17"},
  {1, "192.168.1.18"},
};

static int netmx_open_dev(struct device *d)
{
  int i;
  for( i=0 ; i<array_size(resources) ; i++ )
  {
    if( resources[i].avail )
    {
      resources[i].avail = 0;
      d->mixer.res = i+1;
      d->mixer.info = resources[i].addr;
      return 0;
    }
  }
  return -1;
}

static int netmx_close_dev(struct device *d)
{
  if( d->mixer.res > 0 && d->mixer.res <= array_size(resources) )
  {
    resources[d->mixer.res-1].avail = 1;
    d->mixer.res = 0;
    d->mixer.info = NULL;
  }
  return 0;
}

static int netmx_put(struct device *d __unused, struct packet *pack __unused)
{
  /* return 1 immediately to tell the caller
   * that the buffer is done to use. */
  return 1;
}

static struct packet *netmx_get(struct tag *t __unused)
{
  /* relax the casting thread. */
  while( 1 )
    sleep(1<<20);
  return NULL;
}

static struct mixer netmx = {
  .open_dev = netmx_open_dev,
  .close_dev = netmx_close_dev,
  .put = netmx_put,
  .get = netmx_get,
};

struct mixer *net_mixer = &netmx;

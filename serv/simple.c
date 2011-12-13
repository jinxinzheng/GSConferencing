#include  "mix.h"
#include  "dev.h"

static int simple_put(struct device *d, struct packet *pack)
{
  /* enque packet to device's fifo */
  if( !tag_in_dev_packet(d->tag, d, pack) )
    return 1;
  else
    return 0;
}

static struct packet *simple_get(struct tag *t)
{
  return tag_out_dev_mixed(t);
}

static struct mixer simple = {
  .put = simple_put,
  .get = simple_get,
};

struct mixer *simple_mixer = &simple;

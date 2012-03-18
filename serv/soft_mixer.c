#include  "mix.h"
#include  "dev.h"

static int soft_put(struct device *d, struct packet *pack)
{
  /* enque packet to device's fifo */
  if( !tag_in_dev_packet(d->tag, d, pack) )
    return 1;
  else
    return 0;
}

static struct packet *soft_get(struct tag *t)
{
  return tag_out_dev_mixed(t);
}

static struct mixer soft = {
  .put = soft_put,
  .get = soft_get,
};

struct mixer *soft_mixer = &soft;

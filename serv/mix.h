#ifndef __MIX_H__
#define __MIX_H__

#include  "dev.h"
#include  "packet.h"

struct mixer
{
  int (*open_dev)(struct device *d);
  int (*close_dev)(struct device *d);

  /* return 1 if the packet is done use, otherwise 0. */
  int (*put)(struct device *d, struct packet *p);

  struct packet * (*get)(struct tag *t);
};

/* in soft_mixer.c */
extern struct mixer *soft_mixer;
/* in net_mixer.c */
extern struct mixer *net_mixer;


static inline void set_mixer(struct tag *t, struct mixer *m)
{
  t->mixer = m;
}

#define MIX_OPEN_DEV(dev) \
  if ((dev)->tag->mixer->open_dev) \
    (dev)->tag->mixer->open_dev(dev)

#define MIX_CLOSE_DEV(dev) \
  if ((dev)->tag->mixer->close_dev) \
    (dev)->tag->mixer->close_dev(dev)

#define MIX_PUT(tag,dev,pack)   (tag)->mixer->put(dev,pack)

#define MIX_GET(tag)            (tag)->mixer->get(tag)


#endif  /*__MIX_H__*/

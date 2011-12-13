#ifndef __MIX_H__
#define __MIX_H__

#include  "dev.h"
#include  "packet.h"

struct mixer
{
  /* return 1 if the packet is done use, otherwise 0. */
  int (*put)(struct device *d, struct packet *p);

  struct packet * (*get)(struct tag *t);
};

/* in simple.c */
extern struct mixer *simple_mixer;


void set_mixer(struct mixer *m);

int mix_put(struct device *d, struct packet *p);

struct packet *mix_get(struct tag *t);


#endif  /*__MIX_H__*/

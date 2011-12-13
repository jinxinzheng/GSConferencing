#include  "mix.h"
#include  "init.h"

static struct mixer *mixer;

static int init_mixer()
{
  mixer = simple_mixer;
  return 0;
}
initcall(init_mixer);

void set_mixer(struct mixer *m)
{
  mixer = m;
}

int mix_put(struct device *d, struct packet *p)
{
  return mixer->put(d, p);
}

struct packet *mix_get(struct tag *t)
{
  return mixer->get(t);
}

#include <stdio.h>
#include <assert.h>
#include "strhash.h"

static struct dear
{
  const char *id;

  struct dear *hash_next;
  struct dear **hash_pprev;
}
*h[HASH_SZ];

int main()
{
  struct dear d[4], *p;
  int i;

  d[0].id="0xa";
  d[1].id="0xaa";
  d[2].id="0xaaa";
  d[3].id="0xaaaa";

  hash_id(h, &d[0]);
  hash_id(h, &d[1]);
  hash_id(h, &d[2]);
  hash_id(h, &d[3]);

  for (i=0; i<4; i++){
    p = find_by_id(h, d[i].id);
    assert(p==d+i);
  }

  p = find_by_id(h, "11");
  assert(!p);

  return 0;
}

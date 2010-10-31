#ifndef _STRHASH_H_
#define _STRHASH_H_

#include "hash.h"

static inline unsigned int djbhash(const char *str)
{
  unsigned int hash = 5381;
  while (*str)
    hash += (hash << 5) + (*str++);
  return hash & (HASH_SZ-1);
}

#undef hashfn
#define hashfn(s) (djbhash(s))

#undef equal
#define equal(a,b) (strcmp((a),(b))==0)

#endif

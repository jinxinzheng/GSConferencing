#ifndef _HASH_H_
#define _HASH_H_

/* simple hash code borrowed from linux 2.4 sched.h */

#define HASH_SZ (4096 >> 2)
//extern struct task_struct *pidhash[PIDHASH_SZ];

#define HASH(name, type)  type *name[HASH_SZ]

#define hash_memb(hash, memb, p)              \
do                                            \
{                                             \
  typeof ((hash)[0]) *htable = &(hash)[hashfn((p)->memb)];  \
  if(((p)->hash_next = *htable) != NULL)      \
    (*htable)->hash_pprev = &(p)->hash_next;  \
  *htable = (p);                              \
  (p)->hash_pprev = htable;                   \
} while (0)

#define hash_id(hash, p)  hash_memb(hash, id, p)

#define unhash(p)                                 \
do                                                \
{                                                 \
  if((p)->hash_next)                              \
    (p)->hash_next->hash_pprev = (p)->hash_pprev; \
  *(p)->hash_pprev = (p)->hash_next;              \
} while (0)

#define unhash_id   unhash

#define find_by_memb(hash, memb, val) ({                    \
  typeof ((hash)[0]) p;                                     \
  typeof ((hash)[0]) *htable = &(hash)[hashfn(val)];        \
  for(p = *htable; p && !equal(p->memb, (val)); p = p->hash_next) ; \
  p;                                                        \
})

#define find_by_id(hash, val)   find_by_memb(hash, id, val)

/* other types can override these two functions
 * by #undef and #define */
#define hashfn(x)   ((((x) >> 8) ^ (x)) & (HASH_SZ - 1))

#define equal(a,b) ((a)==(b))

#endif

#include "md.h"
#include <stdlib.h>
#include <string.h>
#include "hash.h"

/* these static buffers store all the data.
 * the p_* pointers points to an 'available space'
 * that could be used to store new data. */

#define SETUP_BUFFERS(type, cap) \
static struct db_##type table_##type[cap]; \
static struct db_##type *p_##type = table_##type; \
static struct db_##type *hash_##type[HASH_SZ];

SETUP_BUFFERS(device, 10000);
SETUP_BUFFERS(tag, 1000);
SETUP_BUFFERS(vote, 1000);

#define CAP(table) (sizeof(table)/sizeof(table[0]))

void md_load_all()
{
  int l,i;

  db_init();

#define LOAD(type) \
  memset(table_##type, 0, sizeof table_##type); \
  memset(hash_##type, 0, sizeof hash_##type); \
  l = db_get_##type(table_##type); \
  for (i=0; i<l; i++) \
    hash_id(hash_##type, &table_##type[i]); \
  p_##type = table_##type + l;

  LOAD(device);

  LOAD(tag);

  LOAD(vote);
}


#define md_find(type, id) \
  find_by_id(hash_##type, (id))


/* p does not need to point to an item in the buffer */
#define md_add(type, p) \
do { \
  if (db_add_##type(p) != 0) \
    break; \
  *p_##type = *(p); \
  hash_id(hash_##type, p_##type); \
  /* find next available space */ \
  { \
    struct db_##type *_t = p_##type; \
    do { \
      _t = (_t+1) % CAP(table_##type); \
      if (_t->id == 0) \
        break; \
    } while (_t != p_##type); \
    if (_t->id == 0) /* found? */ \
      p_##type = _t; \
    else \
      /* if we get here then we are out of buffer. \
       * need to increase the capacity */ \
      fprintf(stderr, "out of buffer\n"); \
  } \
} while(0)


/* p should point to an item in the buffer,
 * i.e. returned by md_find */
#define md_del(type, p) \
do { \
  if (db_del_##type((p)->id) != 0) \
    break; \
  /* delete from hash */ \
  unhash_id(p); \
  /* mark the space available */ \
  (p)->id = 0; \
} while(0)


/* p should point to an item in the buffer,
 * i.e. returned by md_find */
#define md_update(type, p) \
  db_update_##type(p)


struct db_device *md_find_device(long id)
{
  return md_find(device, id);
}

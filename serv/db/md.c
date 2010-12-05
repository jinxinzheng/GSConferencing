#include "md.h"
#include "db.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <include/list.h>
#include <hash.h>

#define SETUP(type) \
struct md_##type                    \
{                                   \
  struct db_##type data;            \
  int id; /* redundant for hash */  \
  struct list_head l;               \
  struct md_##type *hash_next;      \
  struct md_##type **hash_pprev;    \
};                                  \
\
static LIST_HEAD(list_##type);                       \
static struct md_##type *hash_##type[HASH_SZ];       \
\
static void _add_##type(struct db_##type *p)  \
{                                             \
  struct md_##type *m = (struct md_##type *)  \
    malloc(sizeof(struct md_##type));         \
  m->data = *p;                               \
  m->id = m->data.id;                         \
  list_add_tail(&m->l, &list_##type);         \
  hash_id(hash_##type, m);                    \
}                                             \
\
static void _del_##type(int id)   \
{                                 \
  struct md_##type *m =           \
    find_by_id(hash_##type, id);  \
  if (m)                          \
  {                               \
    list_del(&m->l);               \
    unhash_id(m);                 \
    free(m);                      \
  }                               \
}                                 \
\
static int _list_to_array_##type(struct list_head *list, struct db_##type *array[]) \
{                         \
  struct list_head *p;    \
  struct md_##type *m;    \
  int i = 0;              \
                          \
  list_for_each(p, list)  \
  {                       \
    m = list_entry(p, struct md_##type, l); \
                          \
    array[i++] = &m->data;\
  }                       \
  array[i] = NULL;        \
                          \
  return i;               \
}                         \
\
/* get a 'snapshot' of the data */ \
int md_get_array_##type(struct db_##type *array[])    \
{                                                     \
  return _list_to_array_##type(&list_##type, array);  \
}                                                     \
\
struct db_##type *md_find_##type(int id)              \
{                                                     \
  struct md_##type *m = find_by_id(hash_##type, id);  \
  if (m)                                              \
    return &m->data;                                  \
  else                                                \
    return NULL;                                      \
}                                                     \
\
void md_add_##type(struct db_##type *p) \
{                                       \
  if (db_add_##type(p) != 0)            \
    return;                             \
  _add_##type(p);                       \
}                                       \
\
void md_del_##type(int id)    \
{                             \
  if (db_del_##type(id) != 0) \
    return;                   \
  _del_##type(id);            \
}                             \
\
void md_update_##type(struct db_##type *p)    \
{                                             \
  if (db_update_##type(p) != 0)               \
    return;                                   \
  /* assume that p is returned by md_find so
   * we don't need to update memory data. */  \
}


SETUP(device);
SETUP(tag);
SETUP(vote);
SETUP(discuss);

void md_load_all()
{
  int l,i;
  char *buf = malloc(4*1024*1024); /*4M*/

  db_init();

#define LOAD(type) \
  do { \
    struct db_##type *buf_##type; \
    \
    memset(hash_##type, 0, sizeof hash_##type); \
    \
    buf_##type = (struct db_##type *)buf; \
    l = db_get_##type(buf_##type); \
    for (i=0; i<l; i++) \
    { \
      _add_##type(buf_##type+i); \
    } \
  } while(0)

  LOAD(device);
  LOAD(tag);
  LOAD(vote);
  LOAD(discuss);

  free(buf);
}

#include "md.h"
#include "db.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <include/list.h>
#include <include/hash.h>

#if 0
#define md_iterate(type, head, visit) \
do                        \
{                         \
  struct list_head *_p;   \
  struct md_##type *_m;   \
  list_for_each(_p, head) \
  {                       \
    _m = list_entry(_p, struct md_##type, l); \
    visit (&m->data);     \
  }                       \
} while(0)
#endif


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
static int n_##type = 0; \
\
static void _add_##type(struct db_##type *p)  \
{                                             \
  struct md_##type *m = (struct md_##type *)  \
    malloc(sizeof(struct md_##type));         \
  m->data = *p;                               \
  m->id = m->data.id;                         \
  list_add_tail(&m->l, &list_##type);         \
  hash_id(hash_##type, m);                    \
  ++ n_##type;                                \
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
    -- n_##type;                  \
  }                               \
}                                 \
\
static void _clear_##type()                     \
{                                               \
  struct list_head *head = &list_##type;        \
  struct list_head *p, *next;                   \
  struct md_##type *m;                          \
  list_for_each_safe(p, next, head)             \
  {                                             \
    m = list_entry(p, struct md_##type, l);     \
    free(m);                                    \
  }                                             \
  INIT_LIST_HEAD(head);                         \
  memset(hash_##type, 0, sizeof hash_##type);   \
  n_##type = 0;                                 \
}                                               \
\
static int _list_to_array_##type(struct list_head *list, struct db_##type *array[]) \
{                         \
  struct md_##type *m;    \
  int i = 0;              \
                          \
  list_for_each_entry(m, list, l)  \
  {                       \
    array[i++] = &m->data;\
  }                       \
  array[i] = NULL;        \
                          \
  return i;               \
}                         \
\
int md_get_##type##_count() \
{                         \
  return n_##type;        \
}                         \
\
/* get a 'snapshot' of the data */ \
int md_get_array_##type(struct db_##type *array[])    \
{                                                     \
  return _list_to_array_##type(&list_##type, array);  \
}                                                     \
\
struct db_##type *md_iterate_##type##_next(iter *it)\
{                                                   \
  struct list_head *p;                              \
  struct md_##type *m;                              \
  if( !(*it) )                                      \
    p = &list_##type;                               \
  else                                              \
    p = (struct list_head *)*it;                    \
  if( (p = p->next) == &list_##type )               \
    return NULL;                                    \
  *it = p;                                          \
  m = list_entry(p, struct md_##type, l);           \
  return &m->data;                                  \
}                                                   \
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
int md_add_##type(struct db_##type *p) \
{                                      \
  int r;                               \
  if( (r=db_add_##type(p)) != 0 )      \
    return r;                          \
  _add_##type(p);                      \
  return 0;                            \
}                                      \
\
int md_del_##type(int id)    \
{                            \
  int r;                     \
  if( (r=db_del_##type(id)) != 0 ) \
    return r;                \
  _del_##type(id);           \
  return 0;                  \
}                            \
\
int md_update_##type(struct db_##type *p)     \
{                                             \
  int r;                                      \
  if( (r=db_update_##type(p)) != 0 )          \
    return r;                                 \
  /* assume that p is returned by md_find so
   * we don't need to update memory data. */  \
  return 0;                                   \
} \
\
int md_clear_##type()               \
{                                   \
  int r;                            \
  if( (r=db_clear_##type()) != 0 )  \
    return r;                       \
  _clear_##type();                  \
  return 0;                         \
}


SETUP(group);
SETUP(user);
SETUP(device);
SETUP(tag);
SETUP(auth_card);
SETUP(regist);
SETUP(vote);
SETUP(discuss);
SETUP(video);
SETUP(file);

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

  LOAD(group);
  LOAD(user);
  LOAD(device);
  LOAD(tag);
  LOAD(auth_card);
  LOAD(regist);
  LOAD(vote);
  LOAD(discuss);
  LOAD(video);
  LOAD(file);

  free(buf);
}

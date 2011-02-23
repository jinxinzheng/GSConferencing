#ifndef _MD_H_
#define _MD_H_

/* data in memory */

#include "db.h"

void md_load_all();

typedef void *iter;

#define DECLARE(type) \
int md_get_##type##_count(); \
int md_get_array_##type(struct db_##type *array[]); \
void md_iterate_##type##_begin(iter *it);           \
struct db_##type *md_iterate_##type##_next(iter *); \
struct db_##type *md_find_##type(int id);           \
int md_add_##type(struct db_##type *p);            \
int md_del_##type(int id);                         \
int md_update_##type(struct db_##type *p);         \

DECLARE(device);
DECLARE(tag);
DECLARE(vote);
DECLARE(discuss);

#endif

#ifndef _MD_H_
#define _MD_H_

/* data in memory */

#include "db.h"

void md_load_all();

#define DECLARE(type) \
int md_get_array_##type(struct db_##type *array[]); \
struct db_##type *md_find_##type(int id);           \
void md_add_##type(struct db_##type *p);            \
void md_del_##type(int id);                         \
void md_update_##type(struct db_##type *p);         \

DECLARE(device);
DECLARE(tag);
DECLARE(vote);

#endif

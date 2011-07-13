#ifndef __DB_H__
#define __DB_H__


int db_init();

void db_close();

void db_sysconfig_set(const char *key, const char *val);

#include "db_impl.h"


#endif  /*__DB_H__*/

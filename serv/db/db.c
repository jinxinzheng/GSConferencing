#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite.h>
#include "db.h"

static sqlite *db = NULL;
static pthread_mutex_t mut;

int db_init()
{
  char *errmsg;
  db = sqlite_open("sys.db", 0777, &errmsg);
  if (db == 0)
  {
    fprintf(stderr, "Could not open database: %s\n", errmsg);
    sqlite_freemem(errmsg);
    return SQLITE_ERROR;
  }
  else
  {
    printf("Successfully connected to database.\n");
    pthread_mutex_init(&mut, NULL);
    return SQLITE_OK;
  }
}

void db_close()
{
  sqlite_close(db);
}

static int exec_locked(const char *sqlcmd, char **perrmsg)
{
  int ret;
  pthread_mutex_lock(&mut);
  ret = sqlite_exec(db, sqlcmd, NULL, NULL, perrmsg);
  pthread_mutex_unlock(&mut);
  return ret;
}

#if 0
static int db_exec_sql(const char *sql)
{
  char *err;
  return exec_locked(sql, &err);
}

#define db_exec_sqlf(fmt, args...) \
do{ \
  char _sql[1024];            \
  sprintf(_sql, fmt, ##args); \
  db_exec_sql(_sql);          \
}while(0)
#endif


#include "db_impl.c"

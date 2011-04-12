#!/bin/awk -f

BEGIN{
  hfile="db.h"
  cfile="db.c"

  header="_" toupper(hfile) "_"
  gsub(/\./, "_", header)

  print "\
#ifndef "header"\n\
#define "header"\n\
" >hfile

  print "\
#include <stdio.h>\n\
#include <stdlib.h>\n\
#include <sqlite.h>\n\
#include <string.h>\n\
#include \""hfile"\"\n\
\n\
static sqlite *db = NULL;\n\
\n\
int db_init()\n\
{\n\
 char *errmsg;\n\
 db = sqlite_open(\"sys.db\", 0777, &errmsg);\n\
 if (db == 0)\n\
 {\n\
  fprintf(stderr, \"Could not open database: %s\\n\", errmsg);\n\
  sqlite_freemem(errmsg);\n\
  return SQLITE_ERROR;\n\
 }\n\
 else\n\
 {\n\
  printf(\"Successfully connected to database.\\n\");\n\
  return SQLITE_OK;\n\
 }\n\
}\n\
\n\
void db_close()\n\
{\n\
 sqlite_close(db);\n\
}\n\
\n\
" >cfile
}

$1=="table"{
  t=1;
  printf "struct db_%s {\n", $2 >>hfile;
  table=$2;
  i=0;
  gets="";
  updatef="";
  updates="";
  addf=""
  addv=""
  adds=""
}
t&&$1=="i"{
  printf " int %s;\n", $2 >>hfile;
  gets=gets"  rec->"$2" = atoi(values["i++"]);\n";
  if( $2 != "id")
  {
    updatef=updatef $2"=%d,";
    updates=updates "data->"$2",";
  }
  if( $3 != "auto" )
  {
    addf=addf $2",";
    addv=addv "%d,";
    adds=adds "data->"$2",";
  }
}
t&&$1=="s"{
  printf " char %s[%d];\n", $2, $3 >>hfile;
  gets=gets"  strcpy(rec->"$2", values["i++"]);\n";
  updatef=updatef $2"='%s',";
  updates=updates "data->"$2",";
  addf=addf $2",";
  addv=addv "'%s',";
  adds=adds "data->"$2",";
}
t&&$1=="x"{
  # extra fields not in db
  split($0, a, /:/);
  print "/*extra*/ "a[2] >>hfile;
}
t&&/^$/{
  t=0;
  print "};\n" >>hfile;

  updatef=substr(updatef, 0, length(updatef)-1);
  addf=substr(addf,0,length(addf)-1);
  addv=substr(addv,0,length(addv)-1);

  print "\
int db_get_"table"(struct db_"table" *table)\n\
{\n\
 const char **values, **columnNames;\n\
 char *errmsg;\n\
 int ret, cols, rows;\n\
 char sqlcmd[256];\n\
 struct db_"table" *rec = table;\n\
 sqlite_vm *vm;\n\
\n\
 rows = 0;\n\
 strcpy(sqlcmd, \"select * from '"table"';\");\n\
 ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);\n\
\n\
 while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)\n\
 {\n\
"gets"\
  rec++;\n\
  rows++;\n\
 }\n\
\n\
 ret = sqlite_finalize(vm, &errmsg);\n\
\n\
 if (ret != SQLITE_OK)\n\
  fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
 return rows;\n\
}\n\
\n\
int db_update_"table"(struct db_"table" *data)\n\
{\n\
 char *errmsg;\n\
 int ret;\n\
 char sqlcmd[2048];\n\
\n\
 sprintf(sqlcmd, \"update '"table"' set \"\n\
   \""updatef"\"\n\
   \" where id=%d;\",\n\
   "updates"\n\
   data->id);\n\
\n\
 ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);\n\
\n\
 if (ret != SQLITE_OK)\n\
  fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
 return ret;\n\
}\n\
\n\
int db_add_"table"(struct db_"table" *data)\n\
{\n\
  char *errmsg;\n\
  int ret;\n\
  char sqlcmd[2048];\n\
\n\
  sprintf(sqlcmd, \"insert into '"table"' \"\n\
    \"("addf")\"\n\
    \" values("addv");\",\n\
    "adds"\n\
    0/*no harm*/);\n\
\n\
  ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);\n\
\n\
  if (ret != SQLITE_OK)\n\
    fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
  return ret;\n\
}\n\
\n\
int db_del_"table"(int id)\n\
{\n\
  char *errmsg;\n\
  int ret;\n\
  char sqlcmd[64];\n\
\n\
  sprintf(sqlcmd, \"delete from '"table"' where id=%d;\", id);\n\
\n\
  ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);\n\
\n\
  if (ret != SQLITE_OK)\n\
    fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
  return ret;\n\
}\n\
\n\
" >>cfile;
}

END{
  print "#endif" >>hfile
  printf "written %s and %s\n", hfile, cfile
}

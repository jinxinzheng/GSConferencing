#!/bin/awk -f

BEGIN{
  hfile="db_impl.h"
  cfile="db_impl.c"

  header="_" toupper(hfile) "_"
  gsub(/\./, "_", header)

  print "\
#ifndef "header"\n\
#define "header"\n\
" >hfile
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
  field=$2;
  table_type[table, field]="i";

  printf " int %s;\n", $2 >>hfile;
  gets=gets"  if(values["i"]) rec->"$2" = atoi(values["i"]);\n";
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
  i++
}
t&&$1=="s"{
  field=$2;
  table_type[table, field]="s";

  printf " char %s[%d];\n", $2, $3 >>hfile;
  gets=gets"  if(values["i"]) strcpy(rec->"$2", values["i"]);\n";
  updatef=updatef $2"='%s',";
  updates=updates "data->"$2",";
  addf=addf $2",";
  addv=addv "'%s',";
  adds=adds "data->"$2",";
  i++
}
t&&$1=="x"{
  # extra fields not in db
  split($0, a, /:/);
  print "/*extra*/ "a[2] >>hfile;
}
t&&/^$/{
  table_gets[table]=gets;

  t=0;
  print "};\n" >>hfile;

  updatef=substr(updatef, 0, length(updatef)-1);
  addf=substr(addf,0,length(addf)-1);
  addv=substr(addv,0,length(addv)-1);
  adds=substr(adds,0,length(adds)-1);

  get_signature="int db_get_"table"(struct db_"table" *table)";
  update_signature="int db_update_"table"(struct db_"table" *data)";
  add_signature="int db_add_"table"(struct db_"table" *data)";
  delete_signature="int db_del_"table"(int id)";
  clear_signature="int db_clear_"table"()";

  print \
  get_signature ";\n"       \
  update_signature ";\n"    \
  add_signature ";\n"       \
  delete_signature ";\n"    \
  clear_signature ";\n"     >>hfile;

  print "\
" get_signature "\n\
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
" update_signature "\n\
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
 ret = exec_locked(sqlcmd, &errmsg);\n\
\n\
 if (ret != SQLITE_OK)\n\
  fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
 return ret;\n\
}\n\
\n\
" add_signature "\n\
{\n\
  char *errmsg;\n\
  int ret;\n\
  char sqlcmd[2048];\n\
\n\
  sprintf(sqlcmd, \"insert into '"table"' \"\n\
    \"("addf")\"\n\
    \" values("addv");\",\n\
    "adds" );\n\
\n\
  ret = exec_locked(sqlcmd, &errmsg);\n\
\n\
  if (ret != SQLITE_OK)\n\
    fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
  return ret;\n\
}\n\
\n\
" delete_signature "\n\
{\n\
  char *errmsg;\n\
  int ret;\n\
  char sqlcmd[64];\n\
\n\
  sprintf(sqlcmd, \"delete from '"table"' where id=%d;\", id);\n\
\n\
  ret = exec_locked(sqlcmd, &errmsg);\n\
\n\
  if (ret != SQLITE_OK)\n\
    fprintf(stderr, \"SQL error: %s\\n\", errmsg);\n\
\n\
  return ret;\n\
}\n\
\n\
" clear_signature " \
{ \
  char *errmsg; \
  int ret;  \
  char sqlcmd[64];  \
\
  sprintf(sqlcmd, \"delete from '"table"';\"); \
  ret = exec_locked(sqlcmd, &errmsg); \
  if (ret != SQLITE_OK) \
    fprintf(stderr, \"SQL error: %s\\n\", errmsg);  \
\
  return ret; \
}\n\
" >>cfile;
}

$1=="query"{
  table=$2
  arg=$3

  if (table_type[table, arg]=="i")
    argfmt="%d";
  else if (table_type[table, arg]=="s")
    argfmt="'%s'";

  query_signature="int db_get_" table "_by_" arg "(struct db_"table" *data, int max)";

  print query_signature";\n" >>hfile

  print query_signature" \
{ \
 const char **values, **columnNames; \
 char *errmsg; \
 int ret, cols, rows; \
 char sqlcmd[256]; \
 struct db_"table" *rec = data; \
 sqlite_vm *vm; \
\
 rows = 0; \
 sprintf(sqlcmd, \"select * from '"table"' where "arg"="argfmt";\", data->"arg"); \
 ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg); \
\
 while (rows<max && sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW) \
 { \
" table_gets[table]" \
  rec++; \
  rows++; \
 } \
\
 ret = sqlite_finalize(vm, &errmsg); \
 if (ret != SQLITE_OK) \
  fprintf(stderr, \"SQL error: %s\\n\", errmsg); \
 return rows; \
} \
" >>cfile
}

END{
  print "#endif" >>hfile
  printf "written %s and %s\n", hfile, cfile
}

#include "db/md.h"
#include <string.h>
#include <stdio.h>

static struct state_default
{
  const char *name;
  const char *value;
}
state_defaults[] = {
  {NULL, NULL},
  {"discuss",       "0"},
  {"discuss_mode",  "0"},
  {"discuss_id",    "0"}
};

const char *get_state_str(int item)
{
  struct db_state *st;

  if( st = md_find_state(item) )
  {
    return st->value;
  }
  else
  {
    /* return default value */
    return state_defaults[item].value;
  }
}

void set_state_str(int item, const char *val)
{
  struct db_state *st;

  if( st = md_find_state(item) )
  {
    strcpy(st->value, val);
    md_update_state(st);
  }
  else
  {
    struct db_state tmp;
    struct state_default *def = &state_defaults[item];

    tmp.id = item;
    strcpy(tmp.name, def->name);
    strcpy(tmp.value, val);

    md_add_state(&tmp);
  }
}

int get_state_int(int item)
{
  const char *val = get_state_str(item);
  return atoi(val);
}

void set_state_int(int item, int val)
{
  char a[32];
  sprintf(a, "%d", val);
  set_state_str(item, a);
}


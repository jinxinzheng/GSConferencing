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
  {"discuss_id",    "0"},
  {0}
};

static const char *get_state(int id)
{
  struct db_state *st;

  if( st = md_find_state(id) )
  {
    return st->value;
  }
  else
  {
    return NULL;
  }
}

static void set_state(int id, const char *name, const char *val)
{
  struct db_state *st;

  if( st = md_find_state(id) )
  {
    strcpy(st->value, val);
    md_update_state(st);
  }
  else
  {
    struct db_state tmp;

    tmp.id = id;
    strcpy(tmp.name, name);
    strcpy(tmp.value, val);

    md_add_state(&tmp);
  }
}

const char *get_state_str(int item)
{
  const char *val = get_state(item);

  if( val )
    return val;
  else
    /* return default value */
    return state_defaults[item].value;
}

void set_state_str(int item, const char *val)
{
  struct state_default *def = &state_defaults[item];
  set_state(item, def->name, val);
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

const char *get_tag_state(int tid, int item)
{
  int statid = (tid | (item << 24));

  return get_state(statid);
}

void set_tag_state(int tid, int item, const char *val)
{
  int statid = (tid | (item << 24));
  char name[32];

  sprintf(name, "tag %d.%d", tid, item);

  set_state(statid, name, val);
}

#ifndef _CMD_HANDLER_H_
#define _CMD_HANDLER_H_

#include "init.h"
#include "cmd/cmd.h"
#include "sys.h"
#include "devctl.h"
#include "strutil.h"

struct cmd_handler_entry
{
  const char *id; /* the cmd name. use 'id' for hashing. */

  int (*handler)(struct cmd *);

  /* ID hash table linkage. */
  struct cmd_handler_entry *hash_next;
  struct cmd_handler_entry **hash_pprev;
};

void init_cmd_handlers();

void register_cmd_handler(struct cmd_handler_entry *ent);

int handle_cmd(struct cmd *cmd);


#define _CMD_HANDLER_SETUP(name, fn) \
static struct cmd_handler_entry _##fn##_ent =  \
{ \
  .id = name, \
  .handler = fn, \
};  \
static int _##fn##_init()  \
{ \
  register_cmd_handler(&_##fn##_ent);  \
  return 0; \
} \
initcall(_##fn##_init);

#define CMD_HANDLER_SETUP(c)    _CMD_HANDLER_SETUP(#c, cmd_##c)

/* macros helpful to cmd handlers */

#define REP_PRF(cmd, fmt, a...) \
do { \
  (cmd)->rl += sprintf((cmd)->rep+(cmd)->rl, " "fmt, ##a); \
} while(0)

#define REP_ADD_STR(cmd, s, len) \
do { \
  (cmd)->rep[(cmd)->rl++] = ' '; \
  memcpy((cmd)->rep+(cmd)->rl, (s), len); \
  (cmd)->rl += len; \
  (cmd)->rep[(cmd)->rl] = 0;  \
} while(0)

#define REP_ADD(cmd, s) \
do { \
  int _l = strlen(s); \
  REP_ADD_STR(cmd, s, _l);  \
} while(0)

#define REP_ADD_NUM(cmd, n) \
do { \
  (cmd)->rep[(cmd)->rl++] = ' '; \
  (cmd)->rl += sprintf((cmd)->rep+(cmd)->rl, "%d", (n)); \
} while(0)

#define REP_END(cmd) \
do { \
  strcpy((cmd)->rep+(cmd)->rl, "\n"); \
  (cmd)->rl += 1; \
} while(0)

#define REP_OK(cmd) \
do { \
  strcpy((cmd)->rep+(cmd)->rl, " OK\n"); \
  (cmd)->rl += 4; \
} while(0)


#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) return 1;

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)


#define THIS_DEVICE(cmd, d) \
  if (!(d = get_device((cmd)->device_id))) \
    return ERR_NOT_REG;

static inline void send_cmd_to_dev(struct cmd *cmd, struct device *d)
{
  device_cmd(d, cmd->rep, cmd->rl);
}

static inline int send_cmd_to_dev_id(struct cmd *cmd, int id)
{
  struct device *d;
  if( (d = get_device(id)) )
  {
    send_cmd_to_dev(cmd, d);
    return 1;
  }
  else
    return 0;
}

static inline void send_to_group_all(struct cmd *cmd, struct group *g)
{
  struct device *d;

  list_for_each_entry(d, &g->device_head, list)
  {
    send_cmd_to_dev(cmd, d);
  }
}

#define SEND_TO_GROUP_ALL(cmd) \
do { \
  struct device *_d; \
  THIS_DEVICE(cmd, _d); \
  send_to_group_all(cmd, _d->group); \
} while(0)

static inline void send_to_tag_all(struct cmd *cmd, struct tag *t)
{
  struct device *d;

  list_for_each_entry(d, &t->device_head, tlist)
  {
    send_cmd_to_dev(cmd, d);
  }
}

/* char *p must be defined before using this.
 * idlist is modified during iteration.
 * it is safe if idlist is p. */
#define IDLIST_FOREACH(p, idlist) \
  for ( \
    (p) = strtok((idlist), ","); \
    (p); \
    (p) = strtok(NULL, ","))

/* the 'const' version, not modifying the
 * list string. */
#define FOREACH_ID(p, list) \
  for ( \
    (p) = (list); \
    (p)!= (char *)1; \
    (p) = strchr((p), ',')+1 )

static inline void send_to_idlist(struct cmd *cmd, char *idlist)
{
  char *p;
  struct device *d;

  FOREACH_ID(p, idlist)
  {
    if( (d = get_device(atoi(p))) )
      send_cmd_to_dev(cmd, d);
  }
}

#define SEND_TO_IDLIST(cmd, idlist) \
  send_to_idlist(cmd, idlist)

#define MAKE_STRLIST(buf, parr, arrlen, member) \
do { \
  int _i, _l=0; \
  (buf)[0]=0; \
  for (_i=0; _i<(arrlen); _i++) \
  { \
    LIST_ADD(buf, _l, (parr)[_i]->member); \
  } \
} while(0)

#define list_TO_NUMLIST(buf, head, type, lm, member) \
do { \
  int _l = 0; \
  type *_e; \
  (buf)[0]=0; \
  list_for_each_entry(_e, (head), lm) \
  { \
    LIST_ADD_NUM(buf, _l, (int)_e->member); \
  } \
} while(0)

#define ARRAY_TO_NUMLIST(buf, arr, arrlen) \
do { \
  int _i, _l=0; \
  strcpy(buf, "0"); \
  for (_i=0; _i<(arrlen); _i++) \
  { \
    LIST_ADD_NUM(buf, _l, (arr)[_i]); \
  } \
} while(0)


#endif

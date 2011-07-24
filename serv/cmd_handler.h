#ifndef _CMD_HANDLER_H_
#define _CMD_HANDLER_H_

#include "cmd/cmd.h"
#include "sys.h"
#include "async.h"

struct cmd_handler_entry
{
  const char *id; /* the cmd name. use 'id' for hashing. */

  int (*handler)(struct cmd *);

  /* ID hash table linkage. */
  struct cmd_handler_entry *hash_next;
  struct cmd_handler_entry **hash_pprev;
};

void init_cmd_handlers();

int handle_cmd(struct cmd *cmd);

/* macros helpful to cmd handlers */

#define REP_PRF(cmd, fmt, a...) \
do { \
  (cmd)->rl += sprintf((cmd)->rep+(cmd)->rl, " "fmt, ##a); \
} while(0)

#define REP_ADD(cmd, s) \
do { \
  (cmd)->rep[(cmd)->rl++] = ' '; \
  strcpy((cmd)->rep+(cmd)->rl, (s)); \
  (cmd)->rl += strlen(s); \
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


#define SEP_ADD(str, l, sep, fmt, a...) \
do { \
  if( l == 0 ) \
    l += sprintf((str)+(l), fmt, ##a); \
  else \
    l += sprintf((str)+(l), sep fmt, ##a); \
} while(0)

#define LIST_ADD_FMT(str, l, fmt, a...) \
  SEP_ADD(str, l, ",", fmt, ##a)

#define LIST_ADD(str, l, a) \
  LIST_ADD_FMT(str, l, "%s", a)

#define LIST_ADD_NUM(str, l, n) \
  LIST_ADD_FMT(str, l, "%d", n)


#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) return 1;

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)


#define THIS_DEVICE(cmd, d) \
  if (!(d = get_device((cmd)->device_id))) \
    return ERR_NOT_REG;

static inline void send_cmd_to_dev(struct cmd *cmd, struct device *d)
{
  if( d->id != 0 )
  {
    async_sendto_dev(cmd->rep, cmd->rl, d);
  }
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
  struct list_head *e;

  list_for_each(e, &g->device_head)
  {
    d = list_entry(e, struct device, list);
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
  struct list_head *e;

  list_for_each(e, &t->device_head)
  {
    d = list_entry(e, struct device, tlist);
    send_cmd_to_dev(cmd, d);
  }
}

/* char *p must be defined before using this.
 * idlist is modified during iteration.
 * it is safe if idlist is p. */
#define IDLIST_FOREACH_p(idlist) \
  for ( \
    p = strtok(idlist, ","); \
    p; \
    p = strtok(NULL, ","))

static inline void send_to_idlist(struct cmd *cmd, char *idlist)
{
  char *p;
  struct device *d;

  IDLIST_FOREACH_p(idlist)
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
  for (_i=0; _i<(arrlen); _i++) \
  { \
    LIST_ADD(buf, _l, (parr)[_i]->member); \
  } \
} while(0)

#define list_TO_NUMLIST(buf, head, type, lm, member) \
do { \
  int _l = 0; \
  struct list_head *_t; \
  type *_e; \
  list_for_each(_t, (head)) \
  { \
    _e = list_entry(_t, type, lm); \
    LIST_ADD_NUM(buf, _l, (int)_e->member); \
  } \
} while(0)

/* sub cmd handlers.
 * handler should fill the response in cmd->rep. */

int handle_cmd_debug(struct cmd *cmd);
int handle_cmd_reg(struct cmd *cmd);
int handle_cmd_get_tags(struct cmd *cmd);
int handle_cmd_sub(struct cmd *cmd);
int handle_cmd_switch_tag(struct cmd *cmd);
int handle_cmd_regist(struct cmd *cmd);
int handle_cmd_discctrl(struct cmd *cmd);
int handle_cmd_interp(struct cmd *cmd);
int handle_cmd_votectrl(struct cmd *cmd);
int handle_cmd_servicecall(struct cmd *cmd);
int handle_cmd_msgctrl(struct cmd *cmd);
int handle_cmd_videoctrl(struct cmd *cmd);
int handle_cmd_filectrl(struct cmd *cmd);
int handle_cmd_synctime(struct cmd *cmd);
int handle_cmd_sysconfig(struct cmd *cmd);
int handle_cmd_manage(struct cmd *cmd);
int handle_cmd_server_user(struct cmd *cmd);
int handle_cmd_report_cyc_ctl(struct cmd *cmd);

#endif

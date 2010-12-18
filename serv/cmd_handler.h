#ifndef _CMD_HANDLER_H_
#define _CMD_HANDLER_H_

#include "cmd/cmd.h"
#include "sys.h"

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

#define LIST_ADD(str, l, a) \
do { \
  int _j = strlen(a); \
  strncpy((str)+(l), (a), _j); \
  (l) += _j; \
  (str)[(l)++] = ','; \
} while(0)

#define LIST_ADD_NUM(str, l, n) \
do { \
  (l) += sprintf((str)+(l), "%d", (n)); \
  (str)[(l)++] = ','; \
} while(0)

/*remove the trailing ',' if necessary */
#define LIST_END(str,l) \
do { \
  if ((l) > 0) \
    l--; \
  str[l] = 0; \
} while(0)


#define NEXT_ARG(p) \
  if (!(p = cmd->args[a++])) return 1;

#define SUBCMD(c) else if (strcmp(c, subcmd)==0)


#define THIS_DEVICE(cmd, d) \
  if (!(d = get_device((cmd)->device_id))) \
    return 1;

#define SEND_TO_GROUP_ALL(cmd) \
do { \
  struct group *_g; \
  struct device *_d; \
  struct list_head *_t; \
  THIS_DEVICE(cmd, _d); \
  _g = _d->group; \
  list_for_each(_t, &_g->device_head) \
  { \
    _d = list_entry(_t, struct device, list); \
    sendto_dev_tcp((cmd)->rep, (cmd)->rl, _d); \
  } \
} while(0)

/* char *p must be defined before using this.
 * idlist is modified during iteration.
 * it is safe if idlist is p. */
#define IDLIST_FOREACH_p(idlist) \
  for ( \
    p = strtok(idlist, ","); \
    p; \
    p = strtok(NULL, ","))

#define SEND_TO_IDLIST(cmd, idlist) \
  IDLIST_FOREACH_p(idlist) \
  { \
    struct device *_d; \
    if (_d = get_device(atoi(p))) \
      sendto_dev_tcp(cmd->rep, cmd->rl, _d); \
  }

#define MAKE_STRLIST(buf, parr, arrlen, member) \
do { \
  int _i, _l=0; \
  for (_i=0; _i<(arrlen); _i++) \
  { \
    LIST_ADD(buf, _l, (parr)[_i]->member); \
  } \
  LIST_END(buf, _l); \
} while(0)

/* sub cmd handlers.
 * handler should fill the response in cmd->rep. */

int handle_cmd_reg(struct cmd *cmd);
int handle_cmd_sub(struct cmd *cmd);
int handle_cmd_discctrl(struct cmd *cmd);
int handle_cmd_votectrl(struct cmd *cmd);
int handle_cmd_servicecall(struct cmd *cmd);
int handle_cmd_msgctrl(struct cmd *cmd);
int handle_cmd_videoctrl(struct cmd *cmd);
int handle_cmd_filectrl(struct cmd *cmd);

#endif

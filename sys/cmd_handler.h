#ifndef _CMD_HANDLER_H_
#define _CMD_HANDLER_H_

#include "cmd.h"

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

/* sub cmd handlers.
 * handler should fill the response in cmd->rep. */

int handle_cmd_reg(struct cmd *cmd);
int handle_cmd_sub(struct cmd *cmd);
int handle_cmd_votectrl(struct cmd *cmd);

#endif

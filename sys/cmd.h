#ifndef _CMD_H_
#define _CMD_H_

enum {
  CMD_NONE,
  CMD_DEV_REGISTER, /* reg */
  CMD_SUBSCRIBE,    /* sub */
  CMD_MAX
};

struct cmd {
  int device_id;
  int cmd;        /* CMD_* */
  char *args[32]; /* arguments, end with NULL */
};

/* line: command line, ended by '\n' or '\r\n'.
 * cmd: cmd struct.
 *
 * return: 0 if succeed. 1 otherwise;
 *
 * command line format:
 * device_id command_name arg1 arg2 ...
 * example:
 * 101 reg p1101
 * */
int parse_cmd(char *line, struct cmd *pcmd);

#endif

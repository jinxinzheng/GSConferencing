#ifndef _CMD_H_
#define _CMD_H_

struct cmd {
  int device_id;
  const char *cmd;
  char *args[32]; /* arguments, end with NULL */

  /* extra params needed to pass to the handler */
  struct sockaddr_in *saddr; /* source addr of this cmd */
  char *rep; /* the repsponse buffer of this cmd,
                initially the  */
  int rl;    /* initial length of the string in rep */
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

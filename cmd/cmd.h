#ifndef _CMD_H_
#define _CMD_H_

#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
static inline int parse_cmd(char *line, struct cmd *pcmd)
{
  char *p;
  int i;

  p = strtok(line, " \r\n");
  pcmd->device_id = atoi(p);

  p = strtok(NULL, " \r\n");
  if (!p)
    return 1;
  pcmd->cmd=p;

  i=0;
  do
  {
    p = strtok(NULL, " \r\n");
    pcmd->args[i++] = p;
  }
  while (p);

  return 0;
}

#endif

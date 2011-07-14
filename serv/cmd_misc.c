#include "cmd_handler.h"
#include <time.h>

int handle_cmd_synctime(struct cmd *cmd)
{
  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);

  REP_ADD(cmd, "OK");
  REP_ADD_NUM(cmd, (int)t.tv_sec);
  REP_ADD_NUM(cmd, (int)t.tv_nsec);
  REP_END(cmd);

  return 0;
}

#ifndef  __BRCMD_H__
#define  __BRCMD_H__


#include "cmd_handler.h"

void brcast_data_to_all(void *data, int len);

void brcast_cmd_to_all(struct cmd *cmd);
void *brcast_cmd_to_all_loop(struct cmd *cmd);

void brcast_cmd_to_tag_all(struct cmd *cmd, int tagid);

void brcast_cmd_to_multi(struct cmd *cmd, int ids[], int n);

void brcast_cmd_stop(void *arg);


#endif  /*__BRCMD_H__*/

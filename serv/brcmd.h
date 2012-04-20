#ifndef  __BRCMD_H__
#define  __BRCMD_H__


void brcast_cmd_to_all(struct cmd *cmd);
void brcast_cmd_to_multi(struct cmd *cmd);
void brcast_cmd_to_tag_all(struct cmd *cmd, int tagid);


#endif  /*__BRCMD_H__*/

#ifndef __STATE_H__
#define __STATE_H__

enum
{
  STATE_DISC = 1,
  STATE_DISC_MODE,
  STATE_DISC_ID,
};

enum
{
  TAG_STATE_DISC_OPEN = 0x1,
};

const char *get_state_str(int item);
void set_state_str(int item, const char *val);

int get_state_int(int item);
void set_state_int(int item, int val);

const char *get_tag_state(int tid, int item);
void set_tag_state(int tid, int item, const char *val);

#endif  /*__STATE_H__*/

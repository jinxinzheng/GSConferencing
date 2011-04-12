#ifndef __STATE_H__
#define __STATE_H__

enum
{
  STATE_DISC = 1,
  STATE_DISC_MODE,
  STATE_DISC_ID,
};

const char *get_state_str(int item);
void set_state_str(int item, const char *val);

int get_state_int(int item);
void set_state_int(int item, int val);

#endif  /*__STATE_H__*/

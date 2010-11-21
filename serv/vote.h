#ifndef _VOTE_H_
#define _VOTE_H_

struct vote
{
  struct list_head device_head;

  /* count of options */
  int cn_options;

  /* accumulated votes for each option indexed starting from 0.
   * support up to 40 options */
  int results[40];
};

#define vote_add_device(v, d) \
  list_add_tail(&(d)->vote.l, &(v)->device_head)

#endif

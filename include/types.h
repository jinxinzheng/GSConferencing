#ifndef _TYPES_H_
#define _TYPES_H_

/* discussion modes */
enum {
  DISCMODE_AUTO,
  DISCMODE_FIFO,
  DISCMODE_PTT,
  DISCMODE_VOICE,
};

/* vote types */
enum {
  VOTE_COMMON,
  VOTE_1IN5,
  VOTE_SCORE
};

/* service request types */
enum {
  REQUEST_TEA,
  REQUEST_COFFEE,
  REQUEST_WATER,
  REQUEST_TOWEL,
  REQUEST_NAPKIN,
  REQUEST_PEN,
  REQUEST_PAPER,
  REQUEST_OTHER
};


#endif

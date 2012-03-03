#ifndef _TYPES_H_
#define _TYPES_H_

/* device types */
enum {
  DEVTYPE_NONE,
  DEVTYPE_NORMAL,
  DEVTYPE_CHAIR,
  DEVTYPE_INTERP,
  DEVTYPE_OTHER,
};

/* user register modes */
enum {
  REGIST_KEY,
  REGIST_CARD_ANY,
  REGIST_CARD_ID,
};

/* discussion modes */
enum {
  DISCMODE_AUTO,
  DISCMODE_FIFO,
  DISCMODE_PTT,
  DISCMODE_VOICE,
};

/* interpreting mode */
enum {
  INTERP_NO,  /* normal interpret */
  INTERP_OR,  /* replicate original channel */
  INTERP_RE,  /* replicate the interpreter's subscription */
};

/* vote types */
enum {
  VOTE_DECIDE,
  VOTE_SATISF,
  VOTE_SCORE,
  VOTE_CUSTOM,
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

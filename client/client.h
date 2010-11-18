#ifndef _CLIENT_H_
#define _CLIENT_H_

void client_init(int id, const char *serverIP);

/* event callback routine.
 * event: EVENT_*
 * arg1, arg2: event dependent args.
 * return: 0 success, other fail */
typedef int (*event_cb)(int event, void *arg1, void *arg2);

void set_event_callback(event_cb);

/* event codes */

enum {
  /* arg1: int, vote number
   * arg2: int, vote type VOTE_* */
  EVENT_VOTE_START = 1,

  /* arg1: int, vote number
   * arg2: int[], results */
  EVENT_VOTE_SHOWRESULT,

  /* arg1: unused
   * arg2: unused */
  EVENT_VOTE_STOP,

  /* arg1: client id
   * arg2: unused */
  EVENT_VOTE_REMIND,

  /* arg1: int, client id
   * arg2: int, flag */
  EVENT_VOTE_FORBID
};


#define out

/* these functions return 0 for success, other for failure.
 *
 * params declared with *out* is used for returning values.
 * the calling side is responsible for allocating the
 * buffers for the out params. */

/* vote controling */

/* vote types */
enum {
  VOTE_COMMON,
  VOTE_1IN5,
  VOTE_SCORE
};

int votectrl_query(out char *votelist);

int votectrl_select(int vote_num, out int *idlist);

/* vote_type: see enum VOTE_* */
int votectrl_start(int vote_num, out int *vote_type);

int votectrl_result(int vote_num, int result);

int votectrl_status(int vote_num, out int *idlist);

int votectrl_showresult(int vote_num, out int *results);

int votectrl_stop();

int votectrl_remind(int id);

int votectrl_forbid(int id, int flag);

#endif

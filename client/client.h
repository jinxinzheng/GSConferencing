#ifndef _CLIENT_H_
#define _CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

void client_init(int id, const char *serverIP, int localPort);

/* event callback routine.
 * event: EVENT_*
 * arg1, arg2: event dependent args.
 * return: 0 success, other fail */
typedef int (*event_cb)(int event, void *arg1, void *arg2);

void set_event_callback(event_cb);

/* event codes */

enum {
  EVENT_NONE,

  /* arg1: void*, audio data
   * arg2: int, data length */
  EVENT_AUDIO,


  /* arg1: int, discussion number
   * arg2: unused */
  EVENT_DISC_OPEN,

  /* arg1, arg2: unused */
  EVENT_DISC_CLOSE,

  /* arg1, arg2: unused */
  EVENT_DISC_STOP,

  /* arg1: int, client id
   * arg2: unused */
  EVENT_DISC_FORBID,


  /* arg1: int, vote number
   * arg2: int, vote type VOTE_* */
  EVENT_VOTE_START,

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

/* register client to the server */
int reg();

/* subscribe to a channel/tag.
 * pass 0 to channel if unsubscribing. */
int sub(int channel);

/* data casting */

int send_audio(void *buf, int len);

/** command functions **/

/* discussion controling */

int discctrl_query(out char *disclist);

int discctrl_select(int disc_num, out int *idlist);

int discctrl_request();

int discctrl_status(out int *idlist);

int discctrl_close();

int discctrl_stop();

int discctrl_forbid(int id);

/* vote controling */

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

#ifdef __cplusplus
}
#endif

#endif

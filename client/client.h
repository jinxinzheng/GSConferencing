#ifndef _CLIENT_H_
#define _CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

void client_init(int id, int type, const char *serverIP, int localPort);

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

  /* arg1, arg2: unused */
  EVENT_REG_OK,

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
  EVENT_VOTE_FORBID,

  /* arg1: char *, msg string
   * arg2: unused */
  EVENT_MSG,

  /* arg1: char *, file data
   * arg2: int, file length */
  EVENT_FILE,

  /* arg1, arg2: unused */
  EVENT_NEED_REG,
};


#define __out

/* these functions return 0 for success, other for failure.
 *
 * params declared with *__out* is used for returning values.
 * the calling side is responsible for allocating the
 * buffers for the __out params. */

/* register client to the server */
int reg(const char *passwd);
/* asynchronous reg,
 * upon success an EVENT_REG_OK is generated. */
void start_try_reg(const char *passwd);

/* subscribe to a channel/tag.
 * pass 0 to channel if unsubscribing. */
int sub(int channel);

/* data casting */

int send_audio(void *buf, int len);

/** command functions **/

/* discussion controling */

int discctrl_query(__out char *disclist);

int discctrl_select(int disc_num, __out int *idlist);

int discctrl_request();

int discctrl_status(__out int *idlist);

int discctrl_close();

int discctrl_stop();

int discctrl_forbid(int id);

/* vote controling */

int votectrl_query(__out char *votelist);

int votectrl_select(int vote_num, __out int *idlist);

/* vote_type: see enum VOTE_* */
int votectrl_start(int vote_num, __out int *vote_type);

int votectrl_result(int vote_num, int result);

int votectrl_status(int vote_num, __out int *idlist);

int votectrl_showresult(int vote_num, __out int *results);

int votectrl_stop();

int votectrl_remind(int id);

int votectrl_forbid(int id, int flag);

/* service request */

/* request: see enum REQUEST_* */
int servicecall(int request);

/* messaging */

int msgctrl_query(__out int *idlist);

/* idlist: ends with 0.
 * if idlist is NULL, send to all other clients. */
int msgctrl_send(int idlist[], const char *msg);

/* video controling */

int videoctrl_query(__out char *vidlist);

int videoctrl_select(int vid_num);

/* file transfer */

int filectrl_query(__out char *filelist);

int filectrl_select(int file_num);

#ifdef __cplusplus
}
#endif

#endif

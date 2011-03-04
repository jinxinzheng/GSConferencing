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

  /* arg1, arg2: unused */
  EVENT_NEED_REG,

  /* arg1: int, register mode, REGIST_*
   * arg2: unused */
  EVENT_REGIST_START,

  /* arg1, arg2: unused */
  EVENT_REGIST_STOP,

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

  /* arg1: int, 1:forbid, 0:cancel
   * arg2: unused */
  EVENT_DISC_FORBIDALL,

  /* client is kicked by the server.
   * arg1, arg2: unused */
  EVENT_DISC_KICK,


  /* arg1: int, vote number
   * arg2: int, vote type VOTE_* */
  EVENT_VOTE_START,

  /* arg1: int, number of members
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

  /* arg1: int *, device id from
   * arg2: char *, msg string */
  EVENT_MSG,

  /* arg1: int *, device id from
   * arg2: char *, msg string */
  EVENT_BROADCAST_MSG,

  /* arg1: char *, file data
   * arg2: int, file length */
  EVENT_FILE,
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

/* fill audio in the buffer returned by send_audio_start(),
 * then call send_audio_end(). */
void *send_audio_start();

int send_audio_end(int len);

/** command functions **/

/* user register */

/* mode: see enum REGIST_* */
int regist_start(int mode);

int regist_stop();

int regist_status(__out int *expect, __out int *got);

/* mode: enum REGIST_*
 * cardid: only used when mode==REGIST_CARD_ID.
 * card: returns the user's card id when mode!=REGIST_CARD_ID.
 * name: returns the user's name.
 * gender: returns the user's gender. 1 for man, 0 for woman.
 * */
int regist_reg(int mode, int cardid, __out int *card, __out char *name, __out int *gender);

/* discussion controling */

/* mode: enum DISCMODE_* */
int discctrl_set_mode(int mode);

int discctrl_query(__out char *disclist);

int discctrl_select(int disc_num, __out int *idlist);

/* open: 1 open, 0 close. */
int discctrl_request(int open);

int discctrl_status(__out int *idlist);

int discctrl_close();

int discctrl_stop();

int discctrl_forbid(int id);

/* flag: 1 disable, 0 enable. */
int discctrl_disable_all(int flag);

/* vote controling */

int votectrl_query(__out char *votelist);

int votectrl_select(int vote_num, __out int *idlist);

/* vote_type: see enum VOTE_* */
int votectrl_start(int vote_num, __out int *vote_type);

int votectrl_result(int vote_num, int result);

int votectrl_status(int vote_num, __out int *idlist);

int votectrl_showresult(int vote_num, __out int *total, __out int *results);

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

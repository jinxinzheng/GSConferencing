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


struct dev_info
{
  int id;

  char user_id[64]; /* user id of this client. */

  char user_name[64]; /* user name of this client. */
  int user_gender; /* 1:man, 0:woman. */

  int tag;  /* which tag it is in */
  int sub[1];  /* what tags does it subscribe */

  int discuss_mode; /* discuss mode. (DISCMODE_) */
  int discuss_num; /* current discuss number. -1 if no discuss. */
  int discuss_nmembers; /* number of members */
  int discuss_idlist[1024]; /* member IDs */
  char discuss_userlist[2048]; /* member names. comma separated. */
  int discuss_chair; /* 0/1. if this client is the chair man. */
  int discuss_open; /* 0/1, whether this client has opened mic. */

  int regist_start; /* 0/1, whether registration has started. */
  int regist_master; /* 0/1. if this client is the master of the registration. */
  int regist_mode; /* registration mode. (REGIST_) */
  int regist_reg; /* 0/1, whether this client has done regist. */

  int vote_num; /* current vote number. -1 if no vote. */
  int vote_nmembers; /* number of members */
  int vote_idlist[1024]; /* member IDs */
  char vote_userlist[2048]; /* member names. comma separated. */
  int vote_master; /* 0/1. if this client is the master of the vote. */
  int vote_type; /* vote type. (VOTE_) */
  int vote_total; /* total members count of vote. */
  int vote_results[40]; /* vote results */
  int vote_choice; /* choice of this client. -1 if not chosen. */
};

struct tag_info
{
  int id;
  char name[32];
};

struct audio_data
{
  void *data;
  int len;
};

/* event codes */

enum {
  EVENT_NONE,

  /* arg1: int, from which tag the audio was sent.
   * arg2: struct audio_data*, audio data and length. */
  EVENT_AUDIO,

  /* arg1: unused
   * arg2: struct dev_info*, device info. the pointer
   *  is only valid until the event handler returns */
  EVENT_REG_OK,

  /* arg1, arg2: unused */
  EVENT_NEED_REG,

  /* arg1: int, register mode, REGIST_*
   * arg2: unused */
  EVENT_REGIST_START,

  /* arg1, arg2: unused */
  EVENT_REGIST_STOP,

  /* arg1: int, mode, enum DISCMODE_*
   * arg2: unused */
  EVENT_DISC_SETMODE,

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

  /* interpreting mode change.
   * arg1: int, mode (INTERP_*).
   * arg2: unused. */
  EVENT_INTERP_MODE,

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

  /* arg1: char *, pantilt cmd
   * arg2: unused */
  EVENT_PTC,

  /* arg1: char *, cmd string */
  EVENT_SYSCONFIG,

  /* arg1: char *, user id */
  EVENT_SET_UID,
};


#define __out

/* these functions return 0 for success, other for failure.
 *
 * params declared with *__out* is used for returning values.
 * the calling side is responsible for allocating the
 * buffers for the __out params. */

/* register client to the server */
int reg(const char *passwd, __out struct dev_info *info);
/* asynchronous reg,
 * upon success an EVENT_REG_OK is generated. */
void start_try_reg(const char *passwd);

/* tell the server I'm the cyclic controller */
int report_cyc_ctl();

/* synchronize local time with the server */
int synctime();

/* get all tag info */
int get_tags(__out struct tag_info tags[], __out int *count);

/* subscribe to a channel/tag. */
int sub(int tag);

/* unsubscribe a tag */
int unsub(int tag);

/* switch to another channel/tag */
int switch_tag(int tag);

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

int discctrl_set_maxuser(int max);

int discctrl_query(__out char *disclist);

int discctrl_select(int disc_num, __out int *idlist, __out char *users_list);

/* open: 1 open, 0 close. */
int discctrl_request(int open);

int discctrl_status(__out int *idlist);

int discctrl_close();

int discctrl_stop();

int discctrl_forbid(int id);

/* flag: 1 disable, 0 enable. */
int discctrl_disable_all(int flag);

/* interpreting control */

/* mode: enum INTERP_* */
int interp_set_mode(int mode);

/* vote controling */

int votectrl_query(__out char *votelist);

int votectrl_select(int vote_num, __out int *idlist);

/* vote_type: see enum VOTE_* */
int votectrl_start(int vote_num, __out int *vote_type);

int votectrl_result(int vote_num, int result);

int votectrl_status(int vote_num, __out int *total, __out int *voted);

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

int videoctrl_select(int vid_num, __out char *path);

/* file transfer */

int filectrl_query(__out char *filelist);

int filectrl_select(int file_num, __out char *path);

/* server user */

int server_user_login(const char *user, const char *pswd);

int server_user_change_passwd(const char *user, const char *new_pswd);

/* other */

/* get system stats.
 * dev_count: int[4].
 *    dev_count[0] is always zero.
 *    dev_count[1] returns count of normal devs.
 *    dev_count[2] returns count of chair-man devs.
 *    dev_count[3] returns count of interpreter devs. */
int sys_stats(__out int dev_count[]);

/* transparently send command to other clients.
 * id: the other client id. 0 for all other clients.
 * cmd: custom command, can't contain space. */
int sysconfig(int id, const char *cmd);

int set_ptc(int id, int ptcid, const char *ptcmd);

int set_user_id(int id, const char *user_id);

#ifdef __cplusplus
}
#endif

#endif

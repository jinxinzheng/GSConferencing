#include "client.h"
#include "net.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "cmd/cmd.h"
#include "include/queue.h"
#include "include/pack.h"
#include "include/cfifo.h"
#include "../config.h"

#define MINRECV 0
#define MAXRECV 30

static int id;
static int devtype;
static struct sockaddr_in servAddr;
static int listenPort;

static int subscription = 0;
static event_cb event_handler;

#define STREQU(a, b) (strcmp((a), (b))==0)

#define CHECKOK(s) if (!STREQU(s,"OK")) return;

static void handle_cmd(int, char *buf, int l);

static void udp_recved(char *buf, int l);

static struct cfifo udp_snd_fifo;

static struct cfifo udp_rcv_fifo;

static void *run_send_udp(void *arg);

static void *run_recv_udp(void *arg);

void client_init(int dev_id, int type, const char *servIP, int localPort)
{
  int servPort = SERVER_PORT;
  pthread_t thread;

  id = dev_id;

  devtype = type;

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  listenPort = localPort;

  /* listen cmds */
  start_recv_tcp(listenPort, handle_cmd);

  /* listen udp - audio, video and other */
  start_recv_udp(listenPort, udp_recved);

  /* udp sending and recving queues */
  cfifo_init(&udp_snd_fifo, 8, 11); //256 of size and 2K of element size
  cfifo_enable_locking(&udp_snd_fifo);

  cfifo_init(&udp_rcv_fifo, 8, 11);
  cfifo_enable_locking(&udp_rcv_fifo);

  /* udp sender thread */
  pthread_create(&thread, NULL, run_send_udp, NULL);
  pthread_create(&thread, NULL, run_recv_udp, NULL);
}

void set_event_callback(event_cb cb)
{
  event_handler = cb;
}

/* udp casting */

enum {
  TYPE_AUDIO
};

#define HTON P_HTON
#define NTOH P_NTOH

#define headlen(p) ((char*)(*p).data - (char*)p)

static struct pack *audio_current;

void *send_audio_start()
{
  audio_current = (struct pack *)cfifo_get_in(&udp_snd_fifo);
  return audio_current->data;
}

int send_audio_end(int len)
{
  static int qseq = 0;


  audio_current->type = TYPE_AUDIO;
  audio_current->id = (uint32_t)id;
  audio_current->seq = ++qseq;
  audio_current->datalen = (uint32_t)len;


  //enque
  cfifo_in_signal(&udp_snd_fifo);

  return 0;
}

static void *run_send_udp(void *arg)
{
  struct pack *qitem;
  struct list_head *p;
  int l;

  while (1)
  {
    //deque
    cfifo_wait_empty(&udp_snd_fifo);
    qitem = (struct pack *)cfifo_get_out(&udp_snd_fifo);

    //send udp
    l = headlen(qitem) + qitem->datalen;
    HTON(qitem);
    send_udp(qitem, l, &servAddr);

    //free
    cfifo__out(&udp_snd_fifo);
  }
}

static void udp_recved(char *buf, int len)
{
  struct pack *qitem;

  qitem = (struct pack *)buf;
  NTOH(qitem);

  /* broadcasted packet could be sent back */
  if (qitem->id == id)
    return;

  /* broadcasted packet needs to be checked by tag */
  if (qitem->tag != subscription)
    return;

  if (headlen(qitem)+qitem->datalen <= len)
    ;
  else
  {
    fprintf(stderr, "bug: %d+%d > %d\n", headlen(qitem), qitem->datalen, len);
    return; /*mal pack, drop*/
  }

  qitem = (struct pack *)cfifo_get_in(&udp_rcv_fifo);
  memcpy(qitem, buf, len);

  cfifo_in_signal(&udp_rcv_fifo);

  //tmp: do not put in queue, directly play.
  /*if (qitem->type == TYPE_AUDIO)
  {
    event_handler(EVENT_AUDIO,
        (void*)qitem->data,
        (void*)qitem->datalen);
  }*/
}

static void *run_recv_udp(void *arg)
{
  struct pack *qitem;
  struct list_head *p;

  while (1)
  {
    cfifo_wait_empty(&udp_rcv_fifo);
    qitem = (struct pack *)cfifo_get_out(&udp_rcv_fifo);

    /* generate event */
    if (qitem->type == TYPE_AUDIO)
    {
      event_handler(EVENT_AUDIO,
        (void*)qitem->data,
        (void*)qitem->datalen);
    }

    //fprintf(stderr, "recv pack %d\n", qitem->seq);

    cfifo__out(&udp_rcv_fifo);
  }
}

/* cmd delegates */

#define BASICS \
  char buf[2048]; \
  int l,i; \
  struct cmd c;

#define SEND_CMD() do {\
  l = send_tcp(buf, l, &servAddr); \
  if (l>0) { \
    int _e;  \
    buf[l] = 0; \
    if (_e = parse_cmd(buf, &c)) \
    { \
      if (_e == ERR_NOT_REG) \
      { \
        event_handler(EVENT_NEED_REG, NULL, NULL); \
      } \
      return -2; \
    } \
  } \
  else return -1; \
} while (0)

#define FIND_OK(cmd) ({ \
  int _i; \
  for (_i=0; _i<32 && (cmd).args[_i]; _i++) \
    if (strcmp("OK", (cmd).args[_i]) == 0) \
      break; \
  _i; \
})

/* checks whether an int is in a int list str */
static inline int int_in_str(int i, char *s)
{
  char *p = strtok(s, ",");
  do
  {
    if (atoi(p)==i)
      return 1;
    p=strtok(NULL, ",");
  }while (p);
  return 0;
}

#define STR_TO_INT_LIST str_to_int_list

static void str_to_int_list(char *s, int *list)
{
  char *p;
  int i;
  i = 0;
  p = strtok(s, ",");
  while (p) {
    list[i++] = atoi(p);
    p = strtok(NULL, ",");
  }
  list[i] = 0;
}

static void int_list_to_str(char *s, int list[])
{
  int i, l=0;
  for (i=0; list[i]; i++) {
    l += sprintf(s+l, "%d", list[i]);
    s[l++] = ',';
  }
  s[l>0? l-1:0] = 0;
}

int reg(const char *passwd)
{
  BASICS;

#define _MAKE_REG() \
  l = sprintf(buf, "%d reg %d %s %d\n", id, devtype, passwd, listenPort)

  _MAKE_REG();

  SEND_CMD();

  return 0;
}

static void try_reg_end(char *reply)
{
  event_handler(EVENT_REG_OK, NULL, NULL);
}

void start_try_reg(const char *passwd)
{
  BASICS;

  _MAKE_REG();

  start_try_send_tcp(buf, l, &servAddr, try_reg_end);
}

int sub(int tag)
{
  BASICS;

  l = sprintf(buf, "%d sub %d\n", id, tag);

  SEND_CMD();

  subscription = tag;

  return 0;
}


int discctrl_query(char *disclist)
{
  BASICS;

  l = sprintf(buf, "%d discctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(disclist, c.args[i+1]);

  return 0;
}

int discctrl_select(int disc_num, int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d discctrl select %d\n", id, disc_num);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

int discctrl_request()
{
  BASICS;

  l = sprintf(buf, "%d discctrl request\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_status(int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d discctrl status\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

int discctrl_close()
{
  BASICS;

  l = sprintf(buf, "%d discctrl close\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_stop()
{
  BASICS;

  l = sprintf(buf, "%d discctrl stop\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int discctrl_forbid(int did)
{
  BASICS;

  l = sprintf(buf, "%d discctrl forbid %d\n", id, did);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}


int votectrl_query(char *votelist)
{
  BASICS;

  l = sprintf(buf, "%d votectrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(votelist, c.args[i+1]);

  return 0;
}

int votectrl_select(int vote_num, int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d votectrl select %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

int votectrl_start(int vote_num, int *vote_type)
{
  BASICS;

  l = sprintf(buf, "%d votectrl start %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  *vote_type = atoi(c.args[i+1]);

  return 0;
}

int votectrl_result(int vote_num, int result)
{
  BASICS;

  l = sprintf(buf, "%d votectrl result %d %d\n", id, vote_num, result);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_status(int vote_num, int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d votectrl status %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], idlist);

  return 0;
}

int votectrl_showresult(int vote_num, int *results)
{
  BASICS;

  l = sprintf(buf, "%d votectrl showresult %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  STR_TO_INT_LIST(c.args[i+1], results);

  return 0;
}

int votectrl_stop()
{
  BASICS;

  l = sprintf(buf, "%d votectrl stop\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_remind(int did)
{
  BASICS;

  l = sprintf(buf, "%d votectrl remind %d\n", id, did);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int votectrl_forbid(int did, int flag)
{
  BASICS;

  l = sprintf(buf, "%d votectrl forbid %d %d\n", id, did, flag);

  SEND_CMD();

  i = FIND_OK(c);

  return 0;
}

int msgctrl_query(int *idlist)
{
  BASICS;

  l = sprintf(buf, "%d msgctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  str_to_int_list(c.args[i+1], idlist);

  return 0;
}

int servicecall(int request)
{
  BASICS;

  l = sprintf(buf, "%d servicecall %d\n", id, request);

  SEND_CMD();

  return 0;
}

int msgctrl_send(int idlist[], const char *msg)
{
  BASICS;
  char tmp[1024];

  if (!idlist)
    strcpy(tmp, "all");
  else
    int_list_to_str(tmp, idlist);

  l = sprintf(buf, "%d msgctrl send %s %s\n", id, tmp, msg);

  SEND_CMD();

  return 0;
}

int videoctrl_query(char *vidlist)
{
  BASICS;

  l = sprintf(buf, "%d videoctrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(vidlist, c.args[i+1]);

  return 0;
}

int videoctrl_select(int vid_num)
{
  BASICS;

  l = sprintf(buf, "%d videoctrl select %d\n", id, vid_num);

  SEND_CMD();

  FIND_OK(c);

  return 0;
}

int filectrl_query(char *filelist)
{
  BASICS;

  l = sprintf(buf, "%d filectrl query\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  strcpy(filelist, c.args[i+1]);

  return 0;
}

int filectrl_select(int file_num)
{
  BASICS;

  l = sprintf(buf, "%d filectrl select %d\n", id, file_num);

  SEND_CMD();

  FIND_OK(c);

  return 0;
}



static void file_transfer(char *data, int *plen, const char *buf, int l)
{
  if (buf)
  {
    memcpy(data+*plen, buf, l);
    *plen += l;
  }
  else
  {
    event_handler(EVENT_FILE, data, (void*)*plen);
    free(data);
  }
}

/* handle recved cmd and generate appropriate events to the client */
static void handle_cmd(int sock, char *buf, int l)
{
  /* this is maintained during each connection.
   * caution: this only supports one thread running tcp recv! */
  static struct
  {
    int sock;

    void (*job)(char *data, int *plen, const char *buf, int l);

    char *data;
    int len;
  }
  contex = { 0, 0, NULL, 0 };

  struct cmd c;
  int i;


  if (contex.sock == 0)
  {
    /* new connection */
    contex.sock = sock;
    contex.job = NULL;
  }
  else
  {
    if (contex.sock == sock)
    {
      /* connection consecutive */
      if (contex.job)
        contex.job(contex.data, &contex.len, buf, l);
      return;
    }
    else if (0 == sock)
    {
      /* connection is closing */
      if (contex.job)
        contex.job(contex.data, &contex.len, NULL, 0);
      contex.sock = 0;
      contex.job = NULL;
      contex.data = NULL;
      return;
    }
  }


  parse_cmd(buf, &c);

  i = 0;

  if (STREQU(c.cmd, "votectrl"))
  {
    int vn;
    char *sub = c.args[i++];
    if (STREQU(sub, "start"))
    {
      int type;
      vn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      type = atoi(c.args[i++]);
      event_handler(EVENT_VOTE_START, (void*)vn, (void*)type);
    }
    else if (STREQU(sub, "showresult"))
    {
      int results[256];
      vn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      STR_TO_INT_LIST(c.args[i++], results);
      event_handler(EVENT_VOTE_SHOWRESULT, (void*)vn, (void*)results);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_STOP, NULL, NULL);
    }
    else if (STREQU(sub, "remind"))
    {
      int did;
      did = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_REMIND, (void*)did, NULL);
    }
    else if (STREQU(sub, "forbid"))
    {
      int did;
      int flag;
      did = atoi(c.args[i++]);
      flag = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_VOTE_REMIND, (void*)did, (void*)flag);
    }
  }

  else if (STREQU(c.cmd, "discctrl"))
  {
    char *sub = c.args[i++];
    if (STREQU(sub, "select"))
    {
      int dn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_OPEN, (void*)dn, NULL);
    }
    else if (STREQU(sub, "close"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_CLOSE, NULL, NULL);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_STOP, NULL, NULL);
    }
    else if (STREQU(sub, "forbid"))
    {
      int did = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_FORBID, (void*)did, NULL);
    }
  }

  else if (STREQU(c.cmd, "msgctrl"))
  {
    char *sub = c.args[i++];
    if (STREQU(sub, "send"))
    {
      char *ids = c.args[i++];
      char *msg = c.args[i++];
      if (strcmp(ids,"all")==0 || int_in_str(id, ids))
      {
        event_handler(EVENT_MSG, msg, NULL);
      }
    }
  }

  else if (STREQU(c.cmd, "file"))
  {
    /* this is a file transfer. need to handle specially. */
    char *p = c.args[i++];
    int len = atoi(p);
    contex.data = malloc(len);
    contex.len = 0;
    /* put the connection into 'busy' state
     * so that later file data will be correctly handled. */
    contex.job = file_transfer;
  }
}

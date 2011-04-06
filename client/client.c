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
#include "include/debug.h"
#include "../config.h"

#define MINRECV 0
#define MAXRECV 30

static int id;
static int devtype;
static struct sockaddr_in servAddr;
static int listenPort;

static int subscription = 0;
static event_cb event_handler;

static char br_addr[32];

#define STREQU(a, b) (strcmp((a), (b))==0)

#define CHECKOK(s) if (!STREQU(s,"OK")) return;

static void handle_cmd(int, int isfile, char *buf, int l);

static void udp_recved(char *buf, int l);

static struct cfifo udp_snd_fifo;

static struct cfifo udp_rcv_fifo;

static void *run_heartbeat(void *arg);

static void *run_send_udp(void *arg);

static void *run_recv_udp(void *arg);

void client_init(int dev_id, int type, const char *servIP, int localPort)
{
  int servPort = SERVER_PORT;
  pthread_t thread;

  id = dev_id;

  devtype = type;

  /* get this network's broadcast address */
  strcpy(br_addr, "none");
  get_broadcast_addr(br_addr);

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
  cfifo_init(&udp_snd_fifo, 5, 11); //32 of size and 2K of element size
  cfifo_enable_locking(&udp_snd_fifo);

  cfifo_init(&udp_rcv_fifo, 5, 11);
  cfifo_enable_locking(&udp_rcv_fifo);

  /* udp sender thread */
  pthread_create(&thread, NULL, run_heartbeat, NULL);
  pthread_create(&thread, NULL, run_send_udp, NULL);
  pthread_create(&thread, NULL, run_recv_udp, NULL);
}

void set_event_callback(event_cb cb)
{
  event_handler = cb;
}

/* udp casting */

#define HTON P_HTON
#define NTOH P_NTOH

#define headlen(p) ((char*)(*p).data - (char*)p)

static void *run_heartbeat(void *arg)
{
  struct pack hb;
  int sock;

  /* use a different sock than the audio thread */
  if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
      perror("socket()");

  /* Send data to the server */

  hb.id = id;
  hb.seq = 0;
  hb.type = PACKET_HBEAT;
  hb.datalen = 1;
  HTON(&hb);

  while (1)
  {
    hb.seq = htons( ntohs(hb.seq)+1 );

    sendto(sock, &hb, sizeof hb, 0, (struct sockaddr *)&servAddr, sizeof(servAddr));

    /* send heart beat every 3 seconds */
    sleep(3);
  }
}

static struct pack *audio_current;

void *send_audio_start()
{
  audio_current = (struct pack *)cfifo_get_in(&udp_snd_fifo);
  return audio_current->data;
}

int send_audio_end(int len)
{
  static int qseq = 0;


  audio_current->type = PACKET_AUDIO;
  audio_current->id = (uint32_t)id;
  audio_current->seq = ++qseq;
  audio_current->datalen = (uint32_t)len;

  trace2("%d.%d ", audio_current->id, audio_current->seq);
  DEBUG_TIME_NOW();

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

  /* return immediately when we are not subscribed to any */
  if (subscription == 0)
    return;

  qitem = (struct pack *)buf;
  NTOH(qitem);

  trace2("%d.%d ", qitem->id, qitem->seq);
  DEBUG_TIME_NOW();

  /* broadcasted packet could be sent back */
  //if (qitem->id == id)
  //  return;

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

  trace("%d packs in fifo\n", cfifo_len(&udp_rcv_fifo));

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
    if (qitem->type == PACKET_AUDIO)
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

#define PRINTC(fmt, args...) \
  l = sprintf(buf, "%d " fmt "\n", id, ##args);

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

/* return: position of 'OK' in the reply */
int send_cmd(char *buf, int len, struct cmd *reply)
{
  int i,l;
  struct cmd c;

  l = len;

  SEND_CMD();

  i = FIND_OK(c);

  *reply = c;

  return i;
}

#define SIMPLE_CMD(cmd, subcmd) \
int cmd##_##subcmd() \
{ \
  BASICS; \
  l = sprintf(buf, "%d " #cmd " " #subcmd "\n", id); \
  SEND_CMD(); \
  i = FIND_OK(c); \
  return 0; \
}

#define SIMPLE_CMD_i(cmd, subcmd, i) \
int cmd##_##subcmd(int i) \
{ \
  BASICS; \
  l = sprintf(buf, "%d " #cmd " " #subcmd " %d\n", id, i); \
  SEND_CMD(); \
  i = FIND_OK(c); \
  return 0; \
}

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
  l = sprintf(buf, "%d reg %d %s %d %s\n", id, devtype, passwd, listenPort, br_addr)

#define _POST_REG() \
  /*synctime();*/ \
  subscription = 1;

  _MAKE_REG();

  SEND_CMD();

  _POST_REG();

  return 0;
}

static void try_reg_end(char *reply)
{
  int e;
  struct cmd c;
  if( (e = parse_cmd(reply, &c)) == 0 )
  {
    _POST_REG();
  }
  event_handler(EVENT_REG_OK, (void *)e, NULL);
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


SIMPLE_CMD_i(regist, start, mode);

SIMPLE_CMD(regist, stop);

int regist_status(int *expect, int *got)
{
  BASICS;

  l = sprintf(buf, "%d regist status\n", id);

  SEND_CMD();

  i = FIND_OK(c);

  *expect = atoi(c.args[++i]);
  *got    = atoi(c.args[++i]);

  return 0;
}

int regist_reg(int mode, int cardid, int *card, char *name, int *gender)
{
  BASICS;

  l = sprintf(buf, "%d regist reg %d %d\n", id, mode, cardid);

  SEND_CMD();

  i = FIND_OK(c);

  *card   = atoi( c.args[++i]);
  strcpy(name,    c.args[++i]);
  *gender = atoi( c.args[++i]);

  return 0;
}


int discctrl_set_mode(int mode)
{
  BASICS;

  l = sprintf(buf, "%d discctrl setmode %d\n", id, mode);

  SEND_CMD();

  i = FIND_OK(c);

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

int discctrl_request(int open)
{
  BASICS;

  l = sprintf(buf, "%d discctrl request %d\n", id, open);

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

int discctrl_disable_all(int flag)
{
  BASICS;

  l = sprintf(buf, "%d discctrl disableall %d\n", id, flag);

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

int votectrl_showresult(int vote_num, int *total, int *results)
{
  BASICS;

  l = sprintf(buf, "%d votectrl showresult %d\n", id, vote_num);

  SEND_CMD();

  i = FIND_OK(c);

  *total = atoi(c.args[++i]);
  STR_TO_INT_LIST(c.args[++i], results);

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

  i = FIND_OK(c);

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


int synctime()
{
  BASICS;
  struct timespec s,e,t;

  PRINTC("synctime");

  clock_gettime(CLOCK_REALTIME, &s);

  SEND_CMD();

  clock_gettime(CLOCK_REALTIME, &e);

  i = FIND_OK(c);

  t.tv_sec  = atoi(c.args[++i]);
  t.tv_nsec = atoi(c.args[++i]);

  //adjust the round trip time
  t.tv_sec += (e.tv_sec - s.tv_sec)/2;
  t.tv_nsec += (e.tv_nsec - s.tv_nsec)/2;

  if( clock_settime(CLOCK_REALTIME, &t) < 0 )
  {
    perror("clock_settime");
  }

  fprintf(stderr, "server time %s.%s\n", c.args[i-1], c.args[i]);
  fprintf(stderr, "client time %d.%d\n", (int)t.tv_sec, (int)t.tv_nsec);
}


static struct
{
  int len;
  char data[1024*100];
} transfile = {0};

static void handle_file(int sock, char *buf, int l)
{
  if (sock)
  {
    if( transfile.len + l >= sizeof(transfile.data) )
      return;
    memcpy(transfile.data+transfile.len, buf, l);
    transfile.len += l;
  }
  else
  {
    /* file transfer complete */
    event_handler(EVENT_FILE, transfile.data, (void*)transfile.len);
    transfile.len = 0;
  }
}

/* handle recved cmd and generate appropriate events to the client */
static void handle_cmd(int sock, int isfile, char *buf, int l)
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


  if (isfile)
  {
    handle_file(sock, buf, l);
    return;
  }


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
      int nmems;
      int results[256];
      vn = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      nmems = atoi(c.args[i++]);
      STR_TO_INT_LIST(c.args[i++], results);
      event_handler(EVENT_VOTE_SHOWRESULT, (void*)nmems, (void*)results);
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
    else if(STREQU(sub, "disableall"))
    {
      int flag = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_DISC_FORBIDALL, (void*)flag, NULL);
    }
    else if(STREQU(sub, "kick"))
    {
      int kid = atoi(c.args[i++]);
      if( id == kid )
        event_handler(EVENT_DISC_KICK, NULL, NULL);
    }
  }

  else if (STREQU(c.cmd, "regist"))
  {
    char *sub = c.args[i++];
    if (STREQU(sub, "start"))
    {
      int mode = atoi(c.args[i++]);
      CHECKOK(c.args[i++]);
      event_handler(EVENT_REGIST_START, (void*)mode, NULL);
    }
    else if (STREQU(sub, "stop"))
    {
      CHECKOK(c.args[i++]);
      event_handler(EVENT_REGIST_STOP, NULL, NULL);
    }
  }

  else if (STREQU(c.cmd, "msgctrl"))
  {
    char *sub = c.args[i++];

    if (STREQU(sub, "send"))
    {
      int uid = c.device_id;
      char *ids = c.args[i++];
      char *msg = c.args[i++];
      if(STREQU(ids, "all"))
      {
        event_handler(EVENT_BROADCAST_MSG, &uid, msg);
      }
      else
      {
        if (int_in_str(id, ids))
        {
          event_handler(EVENT_MSG, &uid, msg);
        }
      }
    }
  }

  else if (STREQU(c.cmd, "file"))
  {
    /* this is a file transfer. need to handle specially. */
    char *p = c.args[i++];
    int len = atoi(p);
    /* handle file transfer.
     * caution: do not assume the order of the file port and
     * the cmd port. it is random on different archs. */
    transfile.len = 0;
  }
}

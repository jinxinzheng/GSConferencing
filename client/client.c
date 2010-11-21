#include "client.h"
#include "net.h"
#include <string.h>
#include <stdio.h>
#include "cmd/cmd.h"

static int id;
static struct sockaddr_in servAddr;
static int listenPort;

static event_cb event_handler;

#define STREQU(a, b) (strcmp((a), (b))==0)

#define CHECKOK(s) if (!STREQU(s,"OK")) return;

void handle_cmd(char *buf, int l);

void client_init(int dev_id, const char *servIP, int localPort)
{
  int servPort = 7650;

  id = dev_id;

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  listenPort = localPort;

  /* listen cmds */
  start_recv_tcp(listenPort, handle_cmd);
}

void set_event_callback(event_cb cb)
{
  event_handler = cb;
}

#define BASICS \
  char buf[2048]; \
  int l,i; \
  struct cmd c;

#define SEND_CMD() do {\
  l = send_tcp(buf, l, &servAddr); \
  if (l>0) { \
    buf[l] = 0; \
    if (strncmp("FAIL", buf, 4)==0) \
      return -2; \
    parse_cmd(buf, &c); \
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

#define STR_TO_INT_LIST(s, list) do { \
  char *_p; \
  int _i;   \
  _p = (s); \
  _i = 0;   \
  _p = strtok(_p, ","); \
  while (_p) { \
    (list)[_i++] = atoi(_p); \
    _p = strtok(NULL, ",");  \
  } \
  (list)[_i] = 0; \
} while (0)

int reg()
{
  BASICS;

  l = sprintf(buf, "%d reg p%d\n", id, listenPort);

  SEND_CMD();

  return 0;
}

int sub(int tag)
{
  BASICS;

  l = sprintf(buf, "%d sub %d\n", id, tag);

  SEND_CMD();

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

int votectrl_showresult(int vote_num, out int *results)
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

/* handle recved cmd and generate appropriate events to the client */
void handle_cmd(char *buf, int l)
{
  struct cmd c;
  int i;

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
}

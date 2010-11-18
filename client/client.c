#include "client.h"
#include "net.h"
#include <string.h>
#include <stdio.h>
#include <cmd.h>

static int id;
static struct sockaddr_in servAddr;

void client_init(int dev_id, const char *servIP)
{
  int servPort = 7650;

  id = dev_id;

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */
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
  for (_i=0; _i<32 && (cmd).args[_i]; i++) \
    if (strcmp("OK", (cmd).args[i]) == 0) \
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

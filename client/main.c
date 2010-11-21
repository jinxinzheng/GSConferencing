#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "util.h"
#include "net.h"

/* options */
int id=0;
int verbose=0;
static int subscribe=0;
static int udp_auto=0;

static char *servIP = "127.0.0.1";
static int servPort = 7650;

static struct sockaddr_in servAddr;

static void *auto_send_udp(void *arg)
{
  char buf[512], *p;
  int len;

  *(int *)buf = id;
  p = buf+sizeof(id);

  for(;;)
  {
    len = sprintf(p, "%x", rand());
    len += sizeof(int);

    /* Send the string to the server */
    send_udp(buf, len, &servAddr);

    usleep(100000); /*1ms*/
  }
}

static void start_auto_udp()
{
  pthread_t thread;
  pthread_create(&thread, NULL, auto_send_udp, NULL);
}

void read_cmds()
{
  char buf[2048], *cmd, *p;
  int l;

  cmd = buf+16;

  while (fgets(cmd, sizeof(buf)-16, stdin))
  {
    /*prepend id*/
    if (atoi(cmd) != id)
    {
      memset(buf, ' ', 16);
      l = sprintf(buf, "%d", id);
      buf[l] = ' ';
      cmd = buf;
    }

    p = strchr(cmd, '\n');
    l = p-cmd; /* don't include the ending \0 */
    l = send_tcp(cmd, l, &servAddr);
    if (l>0) {
      cmd[l]=0;
      printf("%s\n", cmd);
    }

    cmd = buf+16;
  }
}

int main(int argc, char *const argv[])
{
  int opt;

  char buf[2048];
  int len;

  pthread_t thread;
  int listenPort;

  while ((opt = getopt(argc, argv, "a:i:s:vu")) != -1) {
    switch (opt) {
      case 'a':
        servIP = optarg;
        break;
      case 'i':
        id = atoi(optarg);
        break;
      case 's':
        subscribe = atoi(optarg);
        break;
      case 'v':
        verbose = 1;
        break;
      case 'u':
        udp_auto = 1;
        break;
      default:
        goto USE;
    }
  }

  if (id==0)
    goto USE;

  srand((unsigned int)getpid());

  /* listen packets */

  listenPort = 20000+id;
  start_recv_udp(listenPort, NULL);

  /* listen cmds */
  start_recv_tcp(listenPort, NULL);

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  /* register this client */

  len = sprintf(buf, "%d reg p%d\n", id, listenPort);

  if (send_tcp(buf, len, &servAddr) <= 0)
    return 2;

  /* subscribe to tag */

  if (subscribe != 0)
  {
    len = sprintf(buf, "%d sub %d\n", id, subscribe);

    if (send_tcp(buf, len, &servAddr) != 0)
      return 2;
  }

  /* auto cast packets */
  if (udp_auto)
    start_auto_udp();

  /* this will not stop until eof of stdin */
  read_cmds();

  return 0;

USE:
  fprintf(stderr, "Usage: %s -a addr -i id [-s tag]\n", argv[0]);
  return 1;
}

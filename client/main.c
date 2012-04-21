#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <include/util.h>
#include <include/compiler.h>
#include "net.h"
#include "../include/pack.h"

/* options */
int id=0;
int verbose=0;
static int udp_auto=0;

static char *servIP = "127.0.0.1";
static int servPort = 7650;

static struct sockaddr_in servAddr;

static void *auto_send_udp(void *arg)
{
  char buf[512], *p;
  int len;
  uint32_t s=0;
  struct pack *pack;

  pack = (struct pack *)buf;
  pack->id = htonl((uint32_t)id);

  p = pack->data;

  for(;;)
  {
    pack->seq = htonl(s++);
    pack->type = PACKET_AUDIO;
    len = sprintf(p, "%x", rand());
    len += sizeof(int);

    if (verbose)
      fprintf(stderr, "sending :%s\n", p);

    /* Send the string to the server */
    send_udp(buf, 512, &servAddr);

    usleep(3000); /*1ms=1000*/
  }

  return NULL;
}

static void start_auto_udp()
{
  pthread_t thread;
  pthread_create(&thread, NULL, auto_send_udp, NULL);
}

static void udp_recved(char *buf, int l)
{
  struct pack *pack;
  pack = (struct pack *)buf;
  if( pack->type == PACKET_UCMD )
  {
    struct pack_ucmd *ucmd = (struct pack_ucmd *) pack;
    fprintf(stderr, "ucmd: %s\n", ucmd->data);
  }
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

#if 0
static struct
{
  int id;
  const char *pass;
} passwd[] = {
  {2,   "('(<&+>(=<;<$)$+$(<=<?;$%:(*;;;+"},
  {101, "&:&;+)=+#*=&#&:<$++#(+$<,,&,:,(>"},
  {102, "=+%;'*:)%,&%*?:)<'%<(,>%+)#?$=+,"},
  {103, "=+#??+'#<,:,($;%>$=,'%=*+:)#$(<?"},
  {104, "=?$'<==)$)*%$?,:)+>,;:>&;$+>$>;$"},
  {105, "%$($;?<'%'+#:<:?$<;'?($<=#)($##<"},
  {106, "?>?>?,<?)>==,&<,<>?$#&;'$'');?\?="},
  {107, "(,;:%+=%*$)#<)+;&+#&>($:;:=+&#%$"},
  {108, ";:(,&,)>*;:*>%?:$>+?()=?:),#'<<&"},
  {109, "++&:=))&=><,$$;((),,>(+#;;'%#(*$"},
  {110, ")#+:=)(%''<(:%==(>))>)&(,:+);=+?"},
  {201, "<*())&;,';:<?%>&(*(';>=&%+:+&*=+"},
  {202, "&))#:>&<#:$%&*;<?>%)=%>$'<*)<>$("},
  {203, "?\?>>='<,(?)>'$*%')=$*:#:?>%%),&#"},
  {204, ";$,'%$>)#(%+:(,+;<%<:##+;>##*''%"},
  {205, "'**:():':;&+#(*)<'(<$&:?,)&,*,,<"},
  {206, "?$(+:;=(&$&):*#>::*?$#'#?&,(=<$:"},
  {207, "?)<%&#)=*#*#$+:*)+$%%'#'*$&&'#%+"},
  {208, ">?:?&,;+,%#&,=';=+'(::#=;#);%(<)"},
  {209, "*)*>'(#>%,,$<+)?#%&(,?&*<#*$>*%&"},
  {210, ">=#:''#>?(=*,';#<:',+#&$$>,(<=?;"},
  {0}
};
#endif

static const char *uni = "*&;&&?&'=;:&>;:$=<)?;#$)>=;#)',&";

int main(int argc, char *const argv[])
{
  int opt;

  char buf[2048];
  int len;

  int listenPort;

  int i;

  while ((opt = getopt(argc, argv, "S:i:vu")) != -1) {
    switch (opt) {
      case 'S':
        servIP = optarg;
        break;
      case 'i':
        id = atoi(optarg);
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
  start_recv_udp(listenPort, udp_recved, 1);

  /* listen cmds */
  start_recv_tcp(listenPort, NULL);

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  /* register this client * /
  p = NULL;
  for (i=0; i<sizeof(passwd)/sizeof(passwd[0]); i++)
    if( passwd[i].id == id )
    {
      p = (char*)passwd[i].pass;
      break;
    }

  len = sprintf(buf, "%d reg %d %s %d\n", id, 1, p, listenPort);
  */
  len = sprintf(buf, "%d reg %d %s %d none\n", id, id, uni, listenPort);

  if( (i=send_tcp(buf, len, &servAddr)) <= 0)
    return 2;

  buf[i] = 0;
  printf("%s\n", buf);

  /* auto cast packets */
  if (udp_auto)
    start_auto_udp();

  /* this will not stop until eof of stdin */
  read_cmds();

  return 0;

USE:
  fprintf(stderr, "Usage: %s -S addr -i id [-s tag]\n", argv[0]);
  return 1;
}

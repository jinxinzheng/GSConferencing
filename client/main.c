#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define die(s) do {perror(s); exit(1);} while(0)
#define fail(s) do {perror(s); return -1;} while(0)

static int id=0;
static int subscribe=0;
static int verbose=0;
static char *servIP = "127.0.0.1";
static int servPort = 7650;

static struct sockaddr_in servAddr;

/* connect to and send to the server.
 * the response is stored in buf. */
int send_tcp(void *buf, size_t len, struct sockaddr_in *addr)
{
  int sock;
  char tmp[2048];
  int l;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
    fail("connect()");

  if (send(sock, buf, len, 0) < 0)
    fail("send()");

  /* recv any reply here */
  l=recv(sock, tmp, sizeof tmp, 0);
  if (l > 0)
    memcpy(buf, tmp, l);
  else if(l==0)
    fprintf(stderr, "(no repsponse)\n");
  else
    perror("recv()");

  close(sock);
  return l;
}

int _send_udp(struct sockaddr_in *addr)
{
  int sock;
  char buf[512], *p;
  int len;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  *(int *)buf = id;
  p = buf+sizeof(id);

  for(;;)
  {
    len = sprintf(p, "%x", rand());
    len += sizeof(int);

    /* Send the string to the server */
    if (sendto(sock, buf, len, 0, (struct sockaddr *)
          addr, sizeof(*addr)) != len)
      perror("sendto() failed");

    usleep(100000); /*1ms*/
  }

}

void *run_send_udp(void *addr)
{
  _send_udp((struct sockaddr_in *)addr);
  return NULL;
}

void *run_recv_udp(void *arg)
{
  int sock;
  int port;
  struct sockaddr_in addr; /* Local address */
  struct sockaddr_in other;
  int otherLen;
  char buf[1024], *p;
  int len, i;

  port = (int)arg;

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  /* Construct local address structure */
  memset(&addr, 0, sizeof(addr));   /* Zero out structure */
  addr.sin_family = AF_INET;                /* Internet address family */
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  addr.sin_port = htons(port);      /* Local port */

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    die("bind()");

  printf("%d listen on %d\n", id, port);

  for (;;) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    otherLen = sizeof(other);

    /* Block until receive message from a client */
    if ((len = recvfrom(sock, buf, 1024, 0,
            (struct sockaddr *) &other, &otherLen)) < 0)
    {
      perror("recvfrom() failed");
      continue;
    }

    buf[len] = 0;

    i = *(int *)buf;
    p = buf+sizeof(int);

    if (verbose)
      fprintf(stderr,"(%d) recved %d:%s\n", id, i, p);

  }
  /* NOT REACHED */

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

  /*while (fgets(buf, sizeof(buf), stdin))
  {
    printf(": %s\n", buf);
  }
  return 0;*/

  while ((opt = getopt(argc, argv, "a:i:s:v")) != -1) {
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
      default:
        goto USE;
    }
  }

  if (id==0)
    goto USE;

  srand((unsigned int)getpid());

  /* listen packets */

  listenPort = 20000+id;
  pthread_create(&thread, NULL, run_recv_udp, (void*)listenPort);

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

  /* cast packets, no exit. */
  pthread_create(&thread, NULL, run_send_udp, (void*)&servAddr);

  /* this will not stop until eof of stdin */
  read_cmds();

  return 0;

USE:
  fprintf(stderr, "Usage: %s -a addr -i id [-s tag]\n", argv[0]);
  return 1;
}

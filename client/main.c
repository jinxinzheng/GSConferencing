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

int send_tcp(const void *buf, size_t len, struct sockaddr_in *addr)
{
  int sock;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fail("socket()");

  if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
    fail("connect()");

  if (send(sock, buf, len, 0) < 0)
    fail("send()");

  /* recv any reply here */

  close(sock);
  return 0;
}

int run_send_udp(struct sockaddr_in *addr)
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

    sleep(3);
  }

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

    printf("(%d) recved %d:%s\n", id, i, p);

  }
  /* NOT REACHED */

}

int main(int argc, char *const argv[])
{
  int opt;

  int sock;
  struct sockaddr_in servAddr; /* Echo server address */
  char servIP[] = {"127.0.0.1"};
  int servPort = 7650;
  char buf[1024];
  int len;

  pthread_t thread;
  int listenPort;

  while ((opt = getopt(argc, argv, "i:s:")) != -1) {
    switch (opt) {
      case 'i':
        id = atoi(optarg);
        break;
      case 's':
        subscribe = atoi(optarg);
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
  pthread_create(&thread, NULL, run_recv_udp, (void*)listenPort);

  /* Construct the server address structure */
  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
  servAddr.sin_port        = htons(servPort); /* Server port */

  /* register this client */

  len = sprintf(buf, "%d reg p%d\n", id, listenPort);

  if (send_tcp(buf, len, &servAddr) != 0)
    return 2;

  /* subscribe to tag */

  if (subscribe != 0)
  {
    len = sprintf(buf, "%d sub %d\n", id, subscribe);

    if (send_tcp(buf, len, &servAddr) != 0)
      return 2;
  }

  /* cast packets, no exit. */
  run_send_udp(&servAddr);

USE:
  fprintf(stderr, "Usage: %s -i id [-s tag]\n", argv[0]);
  return 1;
}

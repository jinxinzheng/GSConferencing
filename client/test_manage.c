#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <include/encode.h>
#include <include/util.h>

#define BUFLEN 4096
static int sock;

static void login()
{
  char buf[BUFLEN] = {"0 manage login admin admin\n"};
  int len = strlen(buf);

  unsigned char tmp[BUFLEN];
  int l;

  struct sockaddr_in servAddr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    die("socket()");

  memset(&servAddr, 0, sizeof(servAddr));     /* Zero out structure */
  servAddr.sin_family      = AF_INET;             /* Internet address family */
  servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Server IP address */
  servAddr.sin_port        = htons(7650); /* Server port */

  if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    close(sock);
    die("connect()");
  }

  /* encode the data before sending out */
  l = encode(tmp, buf, len);

  if (send(sock, tmp, l, 0) < 0)
  {
    close(sock);
    die("send()");
  }

  /* recv any reply here */
  l=recv(sock, tmp, sizeof tmp, 0);
  if (l > 0)
  {
    tmp[l] = 0;
    printf("%s\n", tmp);
  }
  else if(l==0)
    fprintf(stderr, "(no repsponse)\n");
  else
    perror("recv()");
}

static void read_cmds()
{
  char buf[BUFLEN], *p;
  int l;

  while( fgets(buf, sizeof(buf), stdin) )
  {
    l = strlen(buf);

    if (send(sock, buf, l, 0) < 0)
    {
      close(sock);
      die("send()");
    }

    l=recv(sock, buf, sizeof buf, 0);
    if (l > 0)
    {
      buf[l] = 0;
      printf("%s\n", buf);
    }
    else if(l==0)
      fprintf(stderr, "(no repsponse)\n");
    else
      perror("recv()");
  }
}

int main(int argc, char *argv[])
{
  login();
  read_cmds();

  return 0;
}

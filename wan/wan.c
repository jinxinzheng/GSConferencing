#include  "../config.h"
#include  <unistd.h>
#include  <string.h>
#include  <stdio.h>

int local_port;
char push_ip[256];
int push_port;
int tcp;
int verbose;

int main(int argc, char *argv[])
{
  int opt;
  int pull_only = 0;
  if( argc < 2 )
  {
    printf("-l local_port: \tlocal port to bind on wan.\n"
      "-s remote: \tsend-to address, 'ip:port'. e.g. 10.0.0.1:1000\n"
      "-c cast_list: \treceived audio cast addresses, e.g. 192.168.1.10:321,0,192.168.1.12:333,...\n"
      "-u : \tuse udp on the internet (default)\n"
      "-t : \tuse tcp on the internet\n"
    );
    return 0;
  }
  while ((opt = getopt(argc, argv, "l:s:c:utvP")) != -1) {
    switch (opt) {
      case 'l':
      {
        local_port = atoi(optarg);
        break;
      }
      case 's':
      {
        char push[256], *p;
        strcpy(push, optarg);
        p = strtok(push, ":");
        strcpy(push_ip, p);
        if( (p = strtok(NULL, ":")) )
          push_port = atoi(p);
        else
          push_port = WAN_PORT;
        break;
      }
      case 'c':
      {
        char addrs[2000];
        strcpy(addrs, optarg);
        set_dec_cast_addrs(addrs);
        break;
      }
      case 'u':
      {
        tcp = 0;
        break;
      }
      case 't':
      {
        tcp = 1;
        break;
      }
      case 'v':
      {
        verbose = 1;
        break;
      }
      case 'P':
      {
        pull_only = 1;
        break;
      }
    }
  }

  if( !pull_only )
    start_collect_data();
  if( push_ip[0] && push_port )
    start_push();
  run_listen();

  while(1)
    sleep(1<<20);
}

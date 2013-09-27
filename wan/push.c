#include  <thread.h>
#include  <stdlib.h>
#include  <lock.h>
#include  "opt.h"
#include  "com.h"

void recv_on_sock(int sock)
{
  char buf[4096];
  int len;
  while(1)
  {
    len = recv_data(sock, buf, sizeof(buf));
    struct com_pack *p = (struct com_pack *)buf;
    //sanity check
    if( p->ver != 0 )
      continue;
    if( len != HEAD_SIZE + p->len )
      continue;
    //deal with received data
    switch (p->cmd)
    {
      case CMD_CONNECT:
        //nothing
        break;
      case CMD_AUDIO:
      {
        recv_audio(p->u.audio.tag, p->u.audio.type,
            p->u.audio.seq, p->u.audio.data,
            p->len - ((char *)&p->u.audio.data - (char *)&p->u.audio));
        break;
      }
    }
  }
}

extern int push_sock;

static void *run_push(void *arg)
{
  push_sock = open_udp_sock(local_port);
  set_push_addr(push_ip, push_port);

  connect_push();

  recv_on_sock(push_sock);
}

void start_push()
{
  if( push_ip[0] && push_port )
  {
    start_thread(run_push, NULL);
  }
}

void run_listen()
{
  if( local_port && !push_port )
  {
    int sock;
    sock = open_udp_sock(local_port);
    recv_on_sock(sock);
  }
}

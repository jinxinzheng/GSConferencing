#include  <thread.h>
#include  <stdlib.h>
#include  <lock.h>
#include  "opt.h"
#include  "com.h"
#include  "log.h"

void recv_on_sock(int sock)
{
  char buf[4096];
  int len;
  while(1)
  {
    len = recv_data(sock, buf, sizeof(buf));
    if( len <= 0 )
    {
      // connection close
      return;
    }
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
        LOG("recvd connect cmd\n");
        break;
      case CMD_AUDIO:
      {
        int datalen = p->len - ((char *)&p->u.audio.data - (char *)&p->u.audio);
        LOG("recvd audio %d of %d\n", datalen, p->u.audio.tag);
        recv_audio(p->u.audio.tag, p->u.audio.type,
            p->u.audio.seq, p->u.audio.data, datalen);
        break;
      }
    }
  }
}

extern int push_sock;
extern int connected;

// client side
static void *run_push(void *arg)
{
  set_push_addr(push_ip, push_port);

  if (tcp)
  {
    while(1)
    {
      push_sock = open_tcp_sock(local_port);
      while( !connect_push() )
      {
        sleep(3);
      }

      recv_on_sock(push_sock);

      connected = 0;
      close(push_sock);
      push_sock = 0;
    }
  }
  else
  {
    push_sock = open_udp_sock(local_port);
    recv_on_sock(push_sock);
  }
}

void start_push()
{
  if( push_ip[0] && push_port )
  {
    start_thread(run_push, NULL);
  }
}

// server side
void run_listen()
{
  if( local_port && !push_port )
  {
    int sock, conn_sock;
    if (tcp)
    {
      sock = open_tcp_sock(local_port);
      listen_sock(sock);
      while (1)
      {
        if( (conn_sock = accept_sock(sock)) < 0 )
        {
          continue;
        }
        push_sock = conn_sock;
        connected = 1;

        recv_on_sock(conn_sock);

        connected = 0;
        close(conn_sock);
        push_sock = 0;
      }
    }
    else
    {
      sock = open_udp_sock(local_port);
      push_sock = sock;
      recv_on_sock(sock);
    }
  }
}

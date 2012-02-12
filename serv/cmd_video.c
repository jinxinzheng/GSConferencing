/* realtime video controling */
#include  "cmd_handler.h"
#include  "network.h"
#include  <include/debug.h>

static struct device *bcast_dev;

static int broadcast_video(const void *buf, int len)
{
  static int sock = 0;

  if( sock <= 0 )
  {
    if( (sock = open_broadcast_sock()) < 0 )
      return 0;
  }

  broadcast_local(sock, buf, len);

  return 1;
}

int cast_video(struct device *d, const void *buf, int len)
{
  if( d == bcast_dev )
  {
    broadcast_video(buf, len);
  }
  else
  {
    trace_warn("unexpected video packet from %d, len=%d\n", d->id, len);
    return -1;
  }
  return 0;
}

static int cmd_video(struct cmd *cmd)
{
  char *subcmd;
  int a=0;
  struct device *d;

  THIS_DEVICE(cmd, d);

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("start")
  {
    char *p;
    int tid;
    NEXT_ARG(p);
    tid = atoi(p);

    if( tid == 0 )  /* broadcast */
    {
      /* only one dev can 'grab' the video channel */
      if( !bcast_dev )
      {
        bcast_dev = d;
      }
      else
        return ERR_REJECTED;
    }
  }

  SUBCMD("stop")
  {
    if( bcast_dev == d )
    {
      bcast_dev = NULL;
    }
    else
      return ERR_OTHER;
  }

  REP_OK(cmd);

  return 0;
}

CMD_HANDLER_SETUP(video);

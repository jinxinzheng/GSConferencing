#include  "client.h"
#include  <linux/soundcard.h>
#include  <sys/ioctl.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <stdlib.h>
#include  <arpa/inet.h>
#include  <include/pack.h>
#include  <include/types.h>
#include  "net.h"
#include  "../config.h"

static int fdw = -1;
static int nonblock = 0;
static int rate = 8000;
static struct sockaddr_in dest_addr;

static int forward_audio_pack(struct pack *p, int len);

int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_AUDIO_RAW :
    {
      /* we have access to the full audio pack
       * since we have demanded it. */
      struct pack *p = (struct pack *) arg1;
      int len = (int) arg2;
      if( fdw > 0 )
      {
        int l = write(fdw, p->data, p->datalen);
        if( l<0 )
          perror("write");
      }
      if( dest_addr.sin_family )
      {
        forward_audio_pack(p, len);
      }
      break;
    }
  }
  return 0;
}

static int forward_audio_pack(struct pack *p, int len)
{
  static uint32_t seq;

  /* re-mangle the pack header */
  p->seq = ++seq;
  P_HTON(p);

  send_udp(p, len, &dest_addr);

  return 0;
}

static int set_format(unsigned int fd, unsigned int bits, unsigned int chn,unsigned int hz)
{
  unsigned int ioctl_val;

  /*reset and sync*/
  ioctl_val=0;
  //ioctl(fd,SNDCTL_DSP_RESET,(char*)&ioctl_val );
  ioctl(fd,SNDCTL_DSP_SYNC,(char*)&ioctl_val);

  /* set bit format */
  ioctl_val = bits;
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &ioctl_val) == -1)
  {
    fprintf(stderr, "Set fmt to bit %d failed:%s\n", bits,strerror(errno));
  }
  if (ioctl_val != bits)
  {
    fprintf(stderr, "do not support bit %d, supported %d\n", bits,ioctl_val);
  }
  /*set channel */
  ioctl_val = chn;
  if ((ioctl(fd, SNDCTL_DSP_CHANNELS, &ioctl_val)) == -1)
  {
    fprintf(stderr, "Set Audio Channels %d failed:%s\n", chn,strerror(errno));
  }

  if (ioctl_val != chn)
  {
    fprintf(stderr, "do not support channel %d,supported %d\n", chn, ioctl_val);
  }

  /*set speed */
  ioctl_val = hz;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &ioctl_val) == -1)
  {
    fprintf(stderr, "Set speed to %d failed:%s\n", hz,strerror(errno));
  }
  if (ioctl_val != hz)
  {
    fprintf(stderr, "do not support speed %d,supported is %d\n", hz,ioctl_val);
  }
  return 0;
}

static void open_audio_out()
{
  int fd;
  int flags = O_WRONLY;
  if( nonblock )
  {
    flags |= O_NONBLOCK;
  }
  fd = open("/dev/dsp", flags, 0777);
  if( fd < 0 )
  {
    perror("open dsp");
  }
  else
  {
    int setting, result;

    ioctl(fd, SNDCTL_DSP_RESET);
    setting = 0x00040009;
    result = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &setting);
    if( result )
    {
      perror("ioctl(SNDCTL_DSP_SETFRAGMENT)");
    }
    set_format(fd, 0x10, 2, rate);

    fdw = fd;
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 11;
  int tag = 1;

  /* the id and server address don't really matter,
   * as we don't register to the server. */

  while ((opt = getopt(argc, argv, "i:S:nf:t:d:")) != -1) {
    switch (opt) {
      case 'i':
        id = atoi(optarg);
        break;
      case 'S':
        srvaddr = optarg;
        break;
      case 'n':
        nonblock = 1;
        break;
      case 'f':
        rate = atoi(optarg);
        break;
      case 't':
        tag = atoi(optarg);
        break;
      case 'd':
        parse_sockaddr_in(&dest_addr, optarg);
        /* default send to the xcast's port if not set. */
        if( dest_addr.sin_port == 0 )
          dest_addr.sin_port = htons(MIX_CAST_PORT);
        break;
    }
  }

  open_audio_out();

  set_option(OPT_ACCESS_RAW_AUDIO_PACK, 1);

  set_event_callback(on_event);

  id |= (tag<<16); //wrap tag and id
  client_init(id, DEVTYPE_NETPLAY, srvaddr, AUDIO_PORT);

  while(1) sleep(10000);

  return 0;
}

#include  "client.h"
#include  <linux/soundcard.h>
#include  <sys/ioctl.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <stdlib.h>
#include  <include/types.h>
#include  "../config.h"

static int fdr = -1;

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

static void open_audio_in()
{
  int fd;
  fd = open("/dev/dsp", O_RDONLY, 0777);
  if( fd > 0 )
  {
    int setting, result;

    ioctl(fd, SNDCTL_DSP_RESET);
    setting = 0x00040009;
    result = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &setting);
    if( result )
    {
      perror("ioctl(SNDCTL_DSP_SETFRAGMENT)");
    }
    set_format(fd, 0x10, 2, 8000);

    fdr = fd;
  }
}

static void run_record()
{
  char *buf;
  int len;
  while(1)
  {
    buf = send_audio_start();
    len = read(fdr, buf, 512);
    if( len<0 )
    {
      perror("read");
      continue;
    }
    send_audio_end(len);
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 0xbca00;
  int tag = 1;

  /* the id and server address don't really matter,
   * as we don't register to the server. */

  while ((opt = getopt(argc, argv, "i:S:t:")) != -1) {
    switch (opt) {
      case 'i':
        id = atoi(optarg);
        break;
      case 'S':
        srvaddr = optarg;
        break;
      case 't':
        tag = atoi(optarg);
        break;
    }
  }

  open_audio_in();

  set_option(OPT_AUDIO_RBUDP, 1);
  /* wrap id and tag */
  client_init(id|tag, DEVTYPE_BCAST_AUDIO, srvaddr, id&0x7fff);

  run_record();

  return 0;
}

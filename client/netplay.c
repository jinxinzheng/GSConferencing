#include  "client.h"
#include  <linux/soundcard.h>
#include  <sys/ioctl.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <stdlib.h>
#include  "../config.h"

static int fdw = -1;

int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_AUDIO :
    {
      if( fdw > 0 )
      {
        int tag;
        struct audio_data *audio;
        int l;
        tag = (int)arg1;
        audio = (struct audio_data *)arg2;
        l = write(fdw, audio->data, audio->len);
      }
      break;
    }
  }
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
  fd = open("/dev/dsp", O_WRONLY, 0777);
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
    set_format(fd, 0x10, 2, 11025);

    fdw = fd;
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 11;

  /* the id and server address don't really matter,
   * as we don't register to the server. */

  while ((opt = getopt(argc, argv, "i:S:")) != -1) {
    switch (opt) {
      case 'i':
        id = atoi(optarg);
        break;
      case 'S':
        srvaddr = optarg;
        break;
    }
  }

  open_audio_out();

  client_init(id, 222, srvaddr, AUDIO_PORT);

  set_event_callback(on_event);

  while(1) sleep(10000);

  return 0;
}
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
#include  "pcm.h"
#include  "adpcm.h"
#include  "aac.h"
#include  <time.h>
#include  "cast.h"

static int fdr = -1;
static int rate = 8000;
static int buf_ord = 0xc;
static int frame_len;

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
    setting = (0x8<<16) | buf_ord;
    result = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &setting);
    if( result )
    {
      perror("ioctl(SNDCTL_DSP_SETFRAGMENT)");
    }
    set_format(fd, 0x10, 2, rate);

    fdr = fd;
  }
}

static void run_record()
{
  char *buf;
  int len;
  while(1)
  {
    buf = get_cast_buffer();
    len = read(fdr, buf, frame_len);
    if( len<0 )
    {
      perror("read");
      continue;
    }
    cast_filled(len);
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 0xbca00;
  int tag = 1;
  int mode = MODE_BROADCAST;
  int repeat = 1;
  int compression = COMPR_NONE;
  const char *dest = NULL;

  /* the id and server address don't really matter,
   * as we don't register to the server. */

  while ((opt = getopt(argc, argv, "i:S:t:f:o:bumc:e:d:")) != -1) {
    switch (opt) {
      case 'i':
        id = atoi(optarg);
        if( (id&255) != 0 )
        {
          /* the lowest byte is reserved for the tag id. */
          id <<= 8;
        }
        break;
      case 'S':
        srvaddr = optarg;
        break;
      case 't':
        tag = atoi(optarg);
        break;
      case 'f':
        rate = atoi(optarg);
        break;
      case 'o':
        buf_ord = atoi(optarg);
        break;
      case 'b':
        mode = MODE_BROADCAST;
        break;
      case 'u':
        mode = MODE_UNICAST;
        break;
      case 'm':
        mode = MODE_MULTICAST;
        break;
      case 'c':
        repeat = atoi(optarg);
        break;
      case 'e':
        compression = atoi(optarg);
        break;
      case 'd':
        dest = optarg;
        break;
    }
  }

  open_audio_in();

  set_cast_mode(mode);

  if( repeat > 1 )
  {
    set_send_repeat(repeat);
  }

  frame_len = set_compression(compression);
  /* use overriden frame length rather than default value.
   * requires the user set the -o buf_ord appropriately. */
  frame_len = 1<<buf_ord;

  if( dest )
  {
    set_ucast_dest(dest);
  }

  /* wrap id and tag */
  client_init(id|tag, DEVTYPE_BCAST_AUDIO, srvaddr, id&0x7fff);

  run_record();

  return 0;
}

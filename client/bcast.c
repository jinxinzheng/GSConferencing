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

static int fdr = -1;
static int rate = 8000;
static int buf_ord = 0xc;
enum {
  COMPR_NONE,
  COMPR_STEREO_TO_MONO,
  COMPR_ADPCM,
  COMPR_AAC,
};
static int compression;

static struct adpcm_state state;
#define FRAMELEN  1024
static char   abuf[FRAMELEN];

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
    buf = send_audio_start();
    if( compression == COMPR_ADPCM )
    {
      len = read(fdr, abuf, FRAMELEN);
      if( len<0 )
      {
        perror("read");
        continue;
      }
      adpcm_coder((short *)abuf, buf, FRAMELEN, &state);
      len = FRAMELEN/4;
    }
    else if( compression == COMPR_AAC )
    {
      char tbuf[4096];
      len = read(fdr, tbuf, sizeof(tbuf));
      if( len<0 )
      {
        perror("read");
        continue;
      }
      //clock_t s,e;
      //s = clock();
      len = aac_encode(buf, 1024, tbuf, len);
      //e = clock();
      //printf("clk %d\n", e-s);
      if( len<0 )
      {
        fprintf(stderr, "encode aac failed\n");
        break;
      }
    }
    else
    {
      len = read(fdr, buf, FRAMELEN);
      if( len<0 )
      {
        perror("read");
        continue;
      }
      if( compression == COMPR_STEREO_TO_MONO )
      {
        len = pcm_stereo_to_mono(buf, len);
      }
    }
    send_audio_end(len);
  }
}

enum {
  MODE_BROADCAST,
  MODE_UNICAST,
  MODE_MULTICAST,
};

int main(int argc, char *const argv[])
{
  int opt;
  char *srvaddr = "127.0.0.1";
  int id = 0xbca00;
  int tag = 1;
  int mode = MODE_BROADCAST;
  int repeat = 1;

  /* the id and server address don't really matter,
   * as we don't register to the server. */

  while ((opt = getopt(argc, argv, "i:S:t:f:o:bumc:e:")) != -1) {
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
    }
  }

  open_audio_in();

  switch ( mode )
  {
    case MODE_BROADCAST : set_option(OPT_AUDIO_RBUDP_SEND, 1); break;
    case MODE_UNICAST :   set_option(OPT_AUDIO_SEND_UCAST, 1); break;
    case MODE_MULTICAST : set_option(OPT_AUDIO_MCAST_SEND, 1); break;
  }

  if( repeat > 1 )
  {
    set_send_repeat(repeat);
  }

  if( compression == COMPR_AAC )
  {
    aacenc_init();
  }

  /* wrap id and tag */
  client_init(id|tag, DEVTYPE_BCAST_AUDIO, srvaddr, id&0x7fff);

  run_record();

  return 0;
}

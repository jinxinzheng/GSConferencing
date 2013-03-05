#include  <linux/soundcard.h>
#include  <sys/ioctl.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <stdlib.h>
#include  <arpa/inet.h>
#include  <include/util.h>
#include  <include/pack.h>
#include  "../config.h"
#include  "master.h"

static int fdr = -1;
static int rate = 8000;
static int buf_ord = 9;
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
  int s, val;
  struct sockaddr_in addr;
  char buf[8192];
  int len;
  struct pack pk;
  int seq;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    die("socket()");

  val = 1;
  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
    die("setsockopt");

  /* setup the broadcast address */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("255.255.255.255");
  addr.sin_port = htons(MASTER_HEART_PORT);

  memset(&pk, 0, sizeof(pk));
  pk.id = MASTER_ID;
  pk.type = PACKET_MASTER_HEART;

  seq = 0;

  while(1)
  {
    len = read(fdr, buf, frame_len);
    if( len<0 )
    {
      perror("read");
      continue;
    }

    if( ++seq < 0 )
      seq = 0;
    pk.seq = seq;

    if( sendto(s, &pk, PACK_HEAD_LEN, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0 )
    {
      perror("sendto");
    }
  }
}

int main(int argc, char *const argv[])
{
  int opt;

  while ((opt = getopt(argc, argv, "f:o:")) != -1) {
    switch (opt) {
      case 'f':
        rate = atoi(optarg);
        break;
      case 'o':
        buf_ord = atoi(optarg);
        break;
    }
  }

  open_audio_in();

  frame_len = 1<<buf_ord;

  run_record();

  return 0;
}

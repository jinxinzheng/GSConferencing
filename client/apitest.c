#include "client.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

static int fdw = -1;

int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_AUDIO :
    {
      static int acount = 0;
      if( ((++acount) & ((1<<8)-1)) == 0 )
      {
        printf("audio from %d, %d\n", (int)arg1, acount);
      }

      if( fdw > 0 )
      {
        struct audio_data *audio = (struct audio_data *)arg2;
        int l = write(fdw, audio->data, audio->len);
      }
    }
    break;

    case EVENT_FILE :
    {
      FILE *f;
      int l;
      f = fopen("transfer", "w");
      l = fwrite(arg1, 1, (int)arg2, f);
      fclose(f);
      printf("file 'transfer' saved\n");
    }
    break;

    default :
    printf("e: %d, %p, %p\n", event, arg1, arg2);
    break;
  }

  return 0;
}

static int set_format(unsigned int fd, unsigned int bits, unsigned int chn,unsigned int hz)
{
  int ioctl_val;

  /*reset and sync*/
  ioctl_val=0;
  ioctl(fd,SNDCTL_DSP_RESET,(char*)&ioctl_val );
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
  fdw = open("/dev/dsp", O_WRONLY, 0777);
  if( fdw > 0 )
  {
    set_format(fdw, 0x10, 2, 11025);
  }
}

int main(int argc, char *const argv[])
{
  int opt;
  int s=0, r=0;
  char *srvaddr = "127.0.0.1";

  int audiotest = 0;

  char buf[2048];
  int i=0;
  int idlist[1000];

  int id=3;

  struct dev_info dev;

  while ((opt = getopt(argc, argv, "srS:a")) != -1) {
    switch (opt) {
      case 's':
        s=1;
        id = 1001;
        break;
      case 'r':
        r=1;
        id = 2001;
        break;
      case 'S':
        srvaddr = optarg;
        break;
      case 'a':
        audiotest = 1;
        break;
    }
  }

  client_init(id,id, srvaddr, 20000+id);

  set_event_callback(on_event);

  start_try_reg("*&;&&?&'=;:&>;:$=<)?;#$)>=;#)',&");
  sleep(1);

  if( audiotest )
  {
    open_audio_out();
    while(1) sleep(10000);
    return 0;
  }

  {
    struct tag_info tags[32];
    int ntag;
    get_tags(tags, &ntag);
  }

  {
    dev_ent_t devs[1024];
    int ndev;
    get_all_devs(devs, &ndev);
  }

  regist_start(0);
  regist_status(idlist+0, idlist+1);
  regist_reg(0, 0, idlist+0, buf, idlist+1);
  regist_stop(idlist+0, idlist+1);

  votectrl_query(buf);

  msgctrl_query(idlist);
  msgctrl_send(NULL, "hello");

  filectrl_query(buf);
  filectrl_select(0, buf);

  interp_set_mode(0);

  //synctime();

  if (r)
  {
    unsub(1);
    unsub(2);
    sub(1);
    sub(2);
  }

  if (s)
  {
    switch_tag(2);

    discctrl_query(buf);
    discctrl_select(0, idlist, buf);
    discctrl_request(1);
  }

  for(;;)
  {
    usleep(2800);
    if (s)
    {
      char *p = send_audio_start();
      sprintf(p, "%x", i++);
      send_audio_end(512);
    }
  }
}

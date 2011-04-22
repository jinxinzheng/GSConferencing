#include "client.h"
#include <unistd.h>
#include <stdio.h>

int on_event(int event, void *arg1, void *arg2)
{
  switch ( event )
  {
    case EVENT_AUDIO :
    {
      printf("audio from %d\n", (int)arg1);
    }
    break;

    case EVENT_FILE :
    {
      FILE *f;
      f = fopen("transfer", "w");
      fwrite(arg1, 1, (int)arg2, f);
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

int main(int argc, char *const argv[])
{
  int opt;
  int s=0, r=0;
  char *srvaddr = "127.0.0.1";

  char buf[2048];
  int i=0;
  int idlist[1000];

  int id=3;

  struct dev_info dev;

  while ((opt = getopt(argc, argv, "srS:")) != -1) {
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
    }
  }

  client_init(id,id, srvaddr, 20000+id);

  set_event_callback(on_event);

  start_try_reg("*&;&&?&'=;:&>;:$=<)?;#$)>=;#)',&");
  sleep(1);


  regist_start(0);
  regist_status(idlist+0, idlist+1);
  regist_reg(0, 0, idlist+0, buf, idlist+1);
  regist_stop();


  msgctrl_query(idlist);
  msgctrl_send(NULL, "hello");

  filectrl_select(0);

  //synctime();

  if (r)
  {
    sub(1);
    unsub(1);
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

#include "client.h"
#include <unistd.h>
#include <stdio.h>

int on_event(int event, void *arg1, void *arg2)
{
  if (event == EVENT_FILE)
  {
    FILE *f;
    f = fopen("transfer", "w");
    fwrite(arg1, 1, (int)arg2, f);
    fclose(f);
    printf("file 'transfer' saved\n");
  }
  else
    printf("e: %d, %s, %d\n", event, (char*)arg1, (int)arg2);
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

  while ((opt = getopt(argc, argv, "srS:")) != -1) {
    switch (opt) {
      case 's':
        s=1;
        id = 101;
        break;
      case 'r':
        r=1;
        id = 201;
        break;
      case 'S':
        srvaddr = optarg;
        break;
    }
  }

  client_init(id,id, srvaddr, 20000+id);

  set_event_callback(on_event);

  reg("*&;&&?&'=;:&>;:$=<)?;#$)>=;#)',&");

  msgctrl_query(idlist);
  msgctrl_send(NULL, "hello");

  filectrl_select(0);

  synctime();

  if (r)
    sub(1);

  if (s)
  {
    discctrl_query(buf);
    discctrl_select(0, idlist);
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

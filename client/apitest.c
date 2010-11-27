#include "client.h"
#include <unistd.h>
#include <stdio.h>

int on_event(int event, void *arg1, void *arg2)
{
  printf("e: %d, %s, %d\n", event, (char*)arg1, (int)arg2);
}

int main(int argc, char *const argv[])
{
  int opt;
  int s=0, r=0;
  char buf[60];
  int i=0;

  int id;

  while ((opt = getopt(argc, argv, "sr")) != -1) {
    switch (opt) {
      case 's':
        s=1;
        id = 101;
        break;
      case 'r':
        r=1;
        id = 201;
        break;
    }
  }

  client_init(id, "127.0.0.1", 20000+id);

  set_event_callback(on_event);

  reg();

  if (r)
    sub(1);

  for(;;)
  {
    sleep(1);
    if (s)
    {
      sprintf(buf, "%x", i++);
      send_audio(buf, 12);
    }
  }
}

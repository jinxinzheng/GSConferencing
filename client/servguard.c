#include  <stdio.h>
#include  <stdlib.h>
#include  <unistd.h>
#include  "client.h"
#include  <include/types.h>

static int on_event(int event, void *arg1, void *arg2)
{
  return 0;
}

int main()
{
  set_event_callback(on_event);
  client_init(0, DEVTYPE_NONE, "127.0.0.1", 0);

  while(1)
  {
    if( test() != 0 )
    {
      /* again */
      if( test() != 0 )
      {
        fprintf(stderr, "restart the server..\n");
        system("killall -9 daya-sys-arm-linux");
        sleep(3);
        system("./daya-sys-arm-linux &");
      }
    }
    sleep(12);
  }

  return 0;
}

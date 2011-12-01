#include  <include/util.h>
#include  <include/debug.h>
#include  "devctl.h"

static int active;
static int allow_chair;
static struct device *chairs[32];

void manage_add_chair(struct device *d)
{
  int i;
  for( i=0 ; i<array_size(chairs) ; i++ )
  {
    if( chairs[i] == d )
    {
      /* already added */
      return;
    }
    else if( !chairs[i] )
    {
      chairs[i] = d;
      return;
    }
  }

  /* we should not get here.. */
  trace_warn("too many chair devices.");
}

void manage_set_active(int a)
{
  active = a;
}

void manage_set_allow_chair(int a)
{
  allow_chair = a;
}

static void __notify_chair(struct device *d, int allow)
{
  char buf[1024];
  int l;

  l = sprintf(buf, "0 allow_chair_ctl %ld %d\n", d->id, allow);
  device_cmd(d, buf, l);
}

void manage_notify_chair(struct device *d)
{
  int allow;
  allow = (!active) || allow_chair;
  __notify_chair(d, allow);
}

void manage_notify_all_chairs()
{
  int i;
  int allow;
  struct device *d;

  allow = (!active) || allow_chair;

  for( i=0 ; i<array_size(chairs) ; i++ )
  {
    if( (d = chairs[i]) )
    {
      __notify_chair(d, allow);
    }
    else
      break;
  }
}

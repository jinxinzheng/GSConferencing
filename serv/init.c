#include "init.h"

extern
initcall_t __initcall_start, __initcall_end;

static void do_initcalls(void)
{
  initcall_t *call;

  call = &__initcall_start;
  do {
    (*call)();
    call++;
  } while (call < &__initcall_end);
}


int init(void)
{
  do_initcalls();

  return 0;
}

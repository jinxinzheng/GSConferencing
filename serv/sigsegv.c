#include <signal.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <include/compiler.h>
#include "init.h"
#define SIZE 1000
static void *buffer[SIZE];

static void fault_trap(int n __unused,struct siginfo *siginfo,void *myact __unused)
{
  int num;
  fprintf(stderr, "Seg Fault address:%p\n",siginfo->si_addr);
  num = backtrace(buffer, SIZE);
  backtrace_symbols_fd(buffer, num, fileno(stderr));
  exit(1);
}

static int setup_trap()
{
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags=SA_SIGINFO;
  act.sa_sigaction=fault_trap;
  sigaction(SIGSEGV,&act,NULL);
  return 0;
}

initcall(setup_trap);

#ifndef __INIT_H__
#define __INIT_H__


#include  <include/compiler.h>

/* init call approach copied from linux
 * which heavily rely on the gcc compiler. */

/*
 * Used for initialization calls..
 */
typedef int (*initcall_t)(void);

#define __init_call     __attribute__ ((section (".initcall.init")))

#define __initcall(fn)                                                \
  static initcall_t __initcall_##fn __used __init_call = fn

#define initcall(fn) __initcall(fn)


int init(void);


#endif  /*__INIT_H__*/

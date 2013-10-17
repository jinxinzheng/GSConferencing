#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

extern int verbose;

#define LOG(fmt, a...)  \
  if(verbose)printf("%s: %s: " fmt, __FILE__, __func__, ##a)

#endif  /*__LOG_H__*/

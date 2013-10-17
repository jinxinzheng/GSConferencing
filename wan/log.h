#ifndef __LOG_H__
#define __LOG_H__

#define DEBUG

#include <stdio.h>

#ifdef DEBUG
#define LOG(fmt, a...) printf("%s: %s: " fmt, __FILE__, __func__, ##a)
#else
#define LOG(...)
#endif

#endif  /*__LOG_H__*/

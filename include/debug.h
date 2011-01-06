#ifndef _DEBUG_H_
#define _DEBUG_H_

#define DEBUG

#ifdef DEBUG
#define trace(fmt, a...) fprintf(stderr, "[%s:%d] " fmt, __FILE__, __LINE__, ##a)
#define debug(expr) expr
#define DEBUG_TIME_INIT() struct timespec s,e
#else
#define trace(fmt, a...)
#define debug(expr)
#define DEBUG_TIME_INIT()
#endif

#define DEBUG_TIME_START() \
  debug(clock_gettime(CLOCK_REALTIME, &s));

#define DEBUG_TIME_STOP(msg) \
  debug(clock_gettime(CLOCK_REALTIME, &e)); \
  trace(msg ": %d.%09d\n", (int)(e.tv_sec-s.tv_sec), (int)(e.tv_nsec-s.tv_nsec));

#endif

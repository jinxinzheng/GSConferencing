#ifndef _DEBUG_H_
#define _DEBUG_H_

#undef DEBUG

#if DEBUG >= 1

 #define trace(fmt, a...) fprintf(stderr, "[%s:%d] [%s] " fmt, __FILE__, __LINE__, __func__, ##a)
 #define debug(expr) expr

#if 0
extern char debug_ring_buf[1024*1024*10];
extern int debug_ring_off;
#define printd(fmt, a...) \
do { \
  debug_ring_off += \
    snprintf(debug_ring_buf+debug_ring_off, \
    , sizeof(debug_ring_buf) - debug_ring_off \
    , fmt, ##a); \
  if( debug_ring_off >= sizeof(debug_ring_buf) ) \
    debug_ring_off = 0; \
} while (0)
#endif

#else /* DEBUG < 1 */
 #define trace(fmt, a...)
 #define debug(expr)
 #define printd(fmt, a...)

#endif /* DEBUG >= 1 */

#if DEBUG >= 2

 #define DEBUG_TIME_INIT() struct timespec _s,_e

 #define DEBUG_TIME_START() \
  clock_gettime(CLOCK_REALTIME, &_s);

 #define DEBUG_TIME_STOP(msg) \
  clock_gettime(CLOCK_REALTIME, &_e); \
  trace(msg ": %d.%09d\n", (int)(_e.tv_sec-_s.tv_sec), (int)(_e.tv_nsec-_s.tv_nsec));

 #define DEBUG_TIME_NOW() \
  { \
    struct timespec _ts; \
    debug(clock_gettime(CLOCK_REALTIME, &_ts)); \
    debug(fprintf(stderr, ": %d.%09d\n", (int)_ts.tv_sec, (int)_ts.tv_nsec)); \
  }

#else /* DEBUG < 2 */
 #define DEBUG_TIME_INIT()
 #define DEBUG_TIME_START()
 #define DEBUG_TIME_STOP(msg)
 #define DEBUG_TIME_NOW()

#endif /* DEBUG >= 2 */

#endif

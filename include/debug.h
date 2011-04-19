#ifndef _DEBUG_H_
#define _DEBUG_H_

#define DEBUG 2

/* debug trace levels:
 * 0: error.
 * 1: warning.
 * 2: info.
 * 3: debug.
 * 4: verbose debug.
 * */

#undef PREFIX_TRACE

#ifdef DEBUG
 #ifdef PREFIX_TRACE
  #define trace(fmt, a...) fprintf(stderr, "[%s:%d] [%s] " fmt, __FILE__, __LINE__, __func__, ##a)
 #else
  #define trace(fmt, a...) fprintf(stderr, fmt, ##a)
 #endif
 #define debug(expr) expr
#else
 #define trace(...)
 #define debug(expr)
#endif

#define trace0 trace

#if DEBUG >= 1

 #define trace1 trace

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
 #define trace1(...)
 #define printd(fmt, a...)

#endif /* DEBUG >= 1 */


#if DEBUG >= 2
 #define trace2 trace
#else
 #define trace2(...)
#endif


#if DEBUG >= 3
 #define trace3 trace
#else
 #define trace3(...)
#endif


#if DEBUG >= 4

 #define trace4 trace

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

#else /* DEBUG < 4 */
 #define trace4(...)
 #define DEBUG_TIME_INIT()
 #define DEBUG_TIME_START()
 #define DEBUG_TIME_STOP(msg)
 #define DEBUG_TIME_NOW()

#endif /* DEBUG >= 4 */


#define trace_err   trace0
#define trace_warn  trace1
#define trace_info  trace2
#define trace_dbg   trace3
#define trace_verb  trace4

#endif

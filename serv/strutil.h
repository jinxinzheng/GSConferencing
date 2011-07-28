#ifndef __STRUTIL_H__
#define __STRUTIL_H__


#define SEP_ADD(str, l, sep, fmt, a...) \
do { \
  if( l == 0 ) \
    l += sprintf((str)+(l), fmt, ##a); \
  else \
    l += sprintf((str)+(l), sep fmt, ##a); \
} while(0)

#define LIST_ADD_FMT(str, l, fmt, a...) \
  SEP_ADD(str, l, ",", fmt, ##a)

#define LIST_ADD(str, l, a) \
  LIST_ADD_FMT(str, l, "%s", a)

#define LIST_ADD_NUM(str, l, n) \
  LIST_ADD_FMT(str, l, "%d", n)


#endif  /*__STRUTIL_H__*/

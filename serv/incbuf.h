#ifndef __INCBUF_H__
#define __INCBUF_H__


/* dynamically increasing buffer */

#define INC_BUF(buf, size, len) do {  \
  if( size < len+200 ) {  \
    size += 4096; \
    buf = realloc(buf, size); \
  } \
} while(0)


#endif  /*__INCBUF_H__*/

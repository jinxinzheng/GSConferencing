#ifndef __THREAD_H__
#define __THREAD_H__


/* creates a detached thread, which does not
 * need to be joined after terminated. */
unsigned long start_thread(void *(*fn)(void *), void *arg);


#endif  /*__THREAD_H__*/

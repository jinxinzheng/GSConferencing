#ifndef __LOCK_H__
#define __LOCK_H__


/* shortcuts to pthread_mutex_(un)lock,
 * so we get rid of the pthread prefix and
 * the address-of operator (does not support pointer).
 * we use upper case to make it look
 * like a macro (not function). */
#define LOCK(lk)    pthread_mutex_lock(&(lk))
#define UNLOCK(lk)  pthread_mutex_unlock(&(lk))


#endif  /*__LOCK_H__*/

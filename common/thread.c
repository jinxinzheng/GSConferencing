#include  <pthread.h>

pthread_t start_thread(void *(*fn)(void *), void *arg)
{
  pthread_attr_t attr;
  pthread_t thread;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_create(&thread, &attr, fn, arg);

  pthread_attr_destroy(&attr);

  return thread;
}

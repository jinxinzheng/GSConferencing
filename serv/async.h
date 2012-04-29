#ifndef __ASYNC_H__
#define __ASYNC_H__

void async_init();

void async_sendto_dev(const void *buf, int len, struct device *d);

void async_run(void (*fn)(void *), void *arg);

#endif  /*__ASYNC_H__*/

#ifndef _CAST_H_
#define _CAST_H_

void *tag_run_casting(void *tag);

int dev_subscribe(struct device *dev, struct tag *tag);

void dev_unsubscribe(struct device *dev);

#endif

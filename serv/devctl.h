#ifndef _DEVCTL_H_
#define _DEVCTL_H_

#include "dev.h"

struct tag *tag_create(long gid, long tid);

struct device *dev_create(long did);

void dev_update_data(struct device *d);

int dev_register(struct device *dev);

int dev_unregister(struct device *dev);

#endif

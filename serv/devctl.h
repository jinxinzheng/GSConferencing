#ifndef _DEVCTL_H_
#define _DEVCTL_H_

#include "dev.h"

struct device *dev_create(long did);

void dev_update_data(struct device *d);

int dev_register(struct device *dev);

int dev_unregister(struct device *dev);

void device_save(struct device *d);

void device_cmd(struct device *d, const char *cmd, int len);

void add_manager_dev();

void dev_activate(struct device *d);
void dev_deactivate(struct device *d);

#endif

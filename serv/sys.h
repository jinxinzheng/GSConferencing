#ifndef _SYS_H_
#define _SYS_H_

#include "dev.h"

int init_groups();

struct group *get_group(int gid);
void add_group(struct group *g);

struct tag *get_tag(int tuid);
void add_tag(struct tag *t);

struct device *get_device(int did);
void add_device(struct device *dev);

void group_add_device(struct group *g, struct device *dev);
void tag_add_device(struct tag *t, struct device *dev);
void tag_del_device(struct device *d);

#endif

#ifndef _SYS_H_
#define _SYS_H_

#include "dev.h"

int init_groups();

struct group *get_group(long gid);
void add_group(struct group *g);

struct tag *get_tag(long long tid);
void add_tag(struct tag *t);

struct device *get_device(long did);
void add_device(struct device *dev);

void group_add_device(struct group *g, struct device *dev);
void tag_add_device(struct tag *t, struct device *dev);
void tag_del_device(struct tag *t, struct device *d);

#endif

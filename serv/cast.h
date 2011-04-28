#ifndef _CAST_H_
#define _CAST_H_

#include "dev.h"

void *tag_run_casting(void *tag);

int dev_subscribe(struct device *dev, struct tag *tag);

void dev_unsubscribe(struct device *dev, struct tag *t);

int dev_cast_packet(struct device *dev, int packet_type, struct packet *pack);

#endif

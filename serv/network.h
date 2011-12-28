#ifndef _NETWORK_H_
#define _NETWORK_H_

#include  "dev.h"

void start_listener_tcp();

void listener_udp();

int dev_send_audio(struct device *d, const void *buf, int len);

int sendto_dev_udp(int sock, const void *buf, size_t len, struct device *dev);

int broadcast_local(int sock, const void *buf, size_t len);
int broadcast(struct tag *t, const void *buf, size_t len);

void sendto_dev_tcp(const void *buf, size_t len, struct device *dev);

void send_file_to_dev(const char *path, struct device *dev);

int ping_dev(struct device *dev);

int open_broadcast_sock();

#endif

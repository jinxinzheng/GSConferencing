#ifndef _NETWORK_H_
#define _NETWORK_H_

void start_listener_tcp();

void listener_udp();

int sendto_dev_udp(int sock, const void *buf, size_t len, struct device *dev);

int broadcast(struct tag *t, const void *buf, size_t len);

void sendto_dev_tcp(const void *buf, size_t len, struct device *dev);

void send_file_to_dev(const char *path, struct device *dev);

#endif

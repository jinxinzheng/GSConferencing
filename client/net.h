#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>

int send_udp(void *buf, size_t len, const struct sockaddr_in *addr);

int send_tcp(void *buf, size_t len, const struct sockaddr_in *addr);

void start_recv_udp(int listenPort, void (*recved)(char *buf, int l));

void start_recv_tcp(int listenPort, void (*recved)(int, char *buf, int l));

#endif

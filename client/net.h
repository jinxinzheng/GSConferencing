#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>

void get_broadcast_addr(char *addr);

int broadcast_udp(void *buf, int len);

int send_udp(void *buf, size_t len, const struct sockaddr_in *addr);

int send_tcp(void *buf, size_t len, const struct sockaddr_in *addr);

void start_try_send_tcp(void *buf, int len, const struct sockaddr_in *addr, void (*try_end)(char *));

enum {
  F_UDP_RECV_BROADCAST  = 1,
  F_UDP_RECV_MULTICAST  = 1<<1,
};

void start_recv_udp(int listenPort, void (*recved)(char *buf, int l), int flag);

void start_recv_tcp(int listenPort, void (*recved)(int, int isfile, char *buf, int l), int recv_file);

int connect_audio_sock(const struct sockaddr_in *addr, int did);
int send_audio(void *buf, size_t len);

/* used when udp multicast recv is enabled. */
int join_mcast_group(in_addr_t maddr);
int quit_mcast_group(in_addr_t maddr);


#endif

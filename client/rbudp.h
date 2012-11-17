#ifndef  __RBUDP_H__
#define  __RBUDP_H__


void rbudp_init_send(int port);
int rbudp_broadcast(int tag, const void *buf, int len, int extra);

void rbudp_init_recv(int port);
void rbudp_set_recv_tag(int tag);
typedef void (*rbudp_recv_cb_t)(int tag, void *buf, int len, int extra);
void rbudp_set_recv_cb(rbudp_recv_cb_t cb);
int rbudp_run_recv();


#endif  /*__RBUDP_H__*/

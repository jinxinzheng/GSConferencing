#include  <thread.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stddef.h>
#include  <pack.h>
#include  <queue.h>
#include  <arpa/inet.h>
#include  "com.h"
#include  "aac.h"
#include  "../config.h"

struct audio {
  struct list_head *queue_node;
  int type;
  int len;
  char data[1];
};

static struct audio_dec_thread {
  pthread_t tid;
  struct blocking_queue queue;
  uint32_t seq;
  int cast_sock;
  struct sockaddr_in cast_addr;
  aacdec_t *h_dec;
} audio_dec_threads[32];

static struct audio_enc_thread {
  pthread_t tid;
  struct blocking_queue queue;
  aacenc_t *h_enc;
} audio_enc_threads[32];

void parse_sockaddr_in(struct sockaddr_in *addr, const char *str)
{
  char tmp[256];
  char *a, *p;
  strcpy(tmp, str);
  a = tmp;
  p = strchr(tmp, ':');
  if(p) *(p++) = 0;
  addr->sin_family      = AF_INET;
  addr->sin_addr.s_addr = inet_addr(a);
  addr->sin_port        = p? htons(atoi(p)) : 0;
}

void set_dec_cast_addrs(char *addrs)
{
  int i = 0;
  char *t = strtok(addrs, ",");
  for( ; t; t=strtok(NULL, ","), i++ )
  {
    if( t[0] == '0' && t[1] == 0 )
    {
      continue;
    }
    parse_sockaddr_in(&audio_dec_threads[i].cast_addr, t);
    if( audio_dec_threads[i].cast_addr.sin_port == 0 )
    {
      audio_dec_threads[i].cast_addr.sin_port = htons(AUDIO_PORT);
    }
  }
}

static void cast_audio(int tag, char *data, int len)
{
  char buf[4096];
  struct pack *pack;
  struct audio_dec_thread *ctx = &audio_dec_threads[tag];
  pack = (struct pack *)buf;
  pack->type = PACKET_AUDIO;
  pack->id = 0;
  pack->seq = ctx->seq++;
  pack->tag = tag;
  pack->datalen = len;
  memcpy(pack->data, data, len);
  P_HTON(pack);

  sendto(ctx->cast_sock, pack, PACK_HEAD_LEN + len, 0,
      (struct sockaddr *)&ctx->cast_addr, sizeof(ctx->cast_addr));
}

static void *run_dec_audio(void *arg)
{
  int tag = (int) arg;
  struct audio_dec_thread *ctx = &audio_dec_threads[tag];
  char buf[4096*2];
  int len;
  struct audio *a;
  while(1)
  {
    struct list_head *h = blocking_deque(&ctx->queue);
    a = list_entry(h, struct audio, queue_node);
    //decode audio
    if( a->type == AUDIO_AAC )
    {
      len = aac_decode(ctx->h_dec, buf, a->data, a->len);
    }
    else if( a->type == AUDIO_AMR )
    {
      len = 320;
    }
    free(a);
    if( len > 0 )
      cast_audio(tag, buf, len);
  }
}

void recv_audio(int tag, int type, int seq, char *data, int len)
{
  struct audio_dec_thread *ctx;
  struct audio *a;
  if( tag > 31 )
  {
    return;
  }
  ctx = &audio_dec_threads[tag];
  if( ctx->tid == 0 )
  {
    blocking_queue_init(&ctx->queue);
    ctx->tid = start_thread(run_dec_audio, (void *)tag);
    ctx->cast_sock = open_udp_sock(0);
    ctx->h_dec = aacdec_init();
  }
  a = (struct audio *) malloc(len + offsetof(struct audio, data));
  a->type = type;
  a->len = len;
  memcpy(a->data, data, len);
  blocking_enque(&ctx->queue, a->queue_node);
}


static void enc_push_audio(int tag, char *buf, int len)
{
  char audio[4096];
  int outlen;
  //encode audio
  outlen = aac_encode(audio_enc_threads[tag].h_enc,
      audio, sizeof(audio), buf, len);
  if( outlen > 0 )
  {
    send_audio(tag, audio, outlen);
  }
}

static void *run_enc_audio(void *arg)
{
  int tag = (int) arg;
  struct audio_enc_thread *ctx = &audio_enc_threads[tag];
  char buf[4096*2];
  int len;
  struct audio *a;
  while(1)
  {
    struct list_head *h = blocking_deque(&ctx->queue);
    a = list_entry(h, struct audio, queue_node);
    memcpy(&buf[len], a->data, a->len);
    len += a->len;
    free(a);
    if( len >= 4096 )
    {
      enc_push_audio(tag, buf, 4096);
      len = 0;
    }
  }
}

static void process_audio(int tag, char *data, int len)
{
  struct audio_enc_thread *ctx;
  struct audio *a;
  if( tag > 31 )
  {
    return;
  }
  ctx = &audio_enc_threads[tag];
  if( ctx->tid == 0 )
  {
    blocking_queue_init(&ctx->queue);
    ctx->tid = start_thread(run_enc_audio, (void *)tag);
    ctx->h_enc = aacenc_init();
  }
  a = (struct audio *) malloc(len + offsetof(struct audio, data));
  a->type = AUDIO_RAW;
  a->len = len;
  memcpy(a->data, data, len);
  blocking_enque(&ctx->queue, a->queue_node);
}

static void *run_collect_data(void *arg)
{
  int sock;
  char buf[4096];
  int len;
  struct pack *pack;

  // open sock bind to AUDIO_PORT
  sock = open_udp_sock(AUDIO_PORT);

  while(1)
  {
    len = recv(sock, buf, sizeof(buf), 0);
    // deal with collected data (audio)
    pack = (struct pack *) buf;
    P_NTOH(pack);
    if( pack->type == PACKET_AUDIO )
    {
      process_audio(pack->tag, pack->data, pack->datalen);
    }
  }

  return NULL;
}

void start_collect_data()
{
  start_thread(run_collect_data, NULL);
}

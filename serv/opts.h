#ifndef _OPTS_H_
#define _OPTS_H_

extern int opt_queue_max;
extern int opt_broadcast;
extern int opt_tcp_audio;
extern int opt_flush;

enum {
  SYNC_WAIT,
  SYNC_FIXED,
  SYNC_REF,
};
extern int opt_sync_policy;

extern int opt_silence_drop;

enum {
  MIXER_SOFT,
  MIXER_NET,
};
extern int opt_mixer;


#endif

#ifndef __PCM_H__
#define __PCM_H__


void pcm_mix(short *pcm_bufs[], int nbufs, int samples);

int pcm_silent(const char *buf, int len, int energy_threshold);

#endif  /*__PCM_H__*/

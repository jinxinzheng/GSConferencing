#ifndef __PCM_H__
#define __PCM_H__


void pcm_mix(short *pcm_bufs[], int nbufs, int samples);

int pcm_silent(const char *buf, int len, int energy_threshold);

int pcm_stereo_to_mono(char *buf, int len);

int pcm_mono_to_stereo(char *buf, int len);


#endif  /*__PCM_H__*/

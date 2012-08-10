#include "pcm.h"

#define BYTES 2

/* mix up to 8 pcm buffers and store mixed data into the
 * first buffer */
void pcm_mix(short *pcm_bufs[], int nbufs, int samples)
{
  const int zero = 1 << ((BYTES*8)-1) ;
  register int mix;
  int i;

  short *bufs[8];
  short **pbuf;
  short *buf0;

  for( i=0 ; i<nbufs ; i++ )
    bufs[i] = pcm_bufs[i];
  buf0 = bufs[0];

  for( i=0 ; i<samples ; i++ )
  {
    mix = 0;
    for( pbuf=&bufs[0] ; pbuf<&bufs[nbufs] ; pbuf++ )
    {
      mix += *( (*pbuf)++ );
    }

    if(mix > zero-1) mix = zero-1;
    else if(mix < -zero) mix = -zero;

    buf0[i] = (short)mix;
  }
}

static inline int abs(int i)
{
  return i>0 ? i:-i;
}

int pcm_silent(const char *buf, int len, int energy_threshold)
{
  const short *pcm = (const short *)buf;
  int pcmlen = len>>1;
  int i;
  register int e=0;

  for( i=0 ; i<pcmlen ; i++ )
  {
    e += abs(pcm[i]);
    if( e > energy_threshold )
    {
      return 0;
    }
  }

  return 1;
}

int pcm_stereo_to_mono(char *buf, int len)
{
  short *ps = (short *) buf;
  int n = len/4;
  int i;
  for( i=0 ; i<n ; i++ )
  {
    ps[i] = ps[i*2];
  }
  return len/2;
}

int pcm_mono_to_stereo(char *buf, int len)
{
  short *ps = (short *) buf;
  int n = len;
  int i;
  for( i=n-2 ; i>=0 ; i-=2 )
  {
    ps[i+1] = ps[i] = ps[i/2];
  }
  return len*2;
}

static int prev_sample[2];

/* only implement 2 channels. */
int pcm_resample_8k_32k(short *dst, const short *src, int samples)
{
  int i,j;
  int s2 = 2*samples;
  register int dis[2], step[2], prev[2], v[2];
  prev[0] = prev_sample[0];
  prev[1] = prev_sample[1];
  j=0;
  for( i=0 ; i<s2 ; i+=2 )
  {
    /* insert before current sample.
     * src[i],
     * dst[j, j+2, j+4, j+6] */
    dis[0] = src[i] - prev[0];
    step[0] = dis[0] >> 2;
    dis[1] = src[i+1] - prev[1];
    step[1] = dis[1] >> 2;

#if 0
    dst[j] = prev[0]+step[0];
    dst[j+2] = dst[j]+step[0];
    dst[j+4] = dst[j+2]+step[0];
    dst[j+6] = dst[j+4]+step[0];

    dst[j+1] = prev[1]+step[1];
    dst[j+3] = dst[j+1]+step[1];
    dst[j+5] = dst[j+3]+step[1];
    dst[j+7] = dst[j+5]+step[1];
#else
    v[0] = prev[0]+step[0];
    v[1] = prev[1]+step[1];
    *(int*) &dst[j] = (v[0]&0xffff) | (v[1]<<16);
    v[0] += step[0];
    v[1] += step[1];
    *(int*) &dst[j+2] = (v[0]&0xffff) | (v[1]<<16);
    v[0] += step[0];
    v[1] += step[1];
    *(int*) &dst[j+4] = (v[0]&0xffff) | (v[1]<<16);
    v[0] += step[0];
    v[1] += step[1];
    *(int*) &dst[j+6] = (v[0]&0xffff) | (v[1]<<16);
#endif
    j+=8;

    //prev[0] = src[i];
    prev[0] += dis[0];
    //prev[1] = src[i+1];
    prev[1] += dis[1];
  }
  prev_sample[0] = prev[0];
  prev_sample[1] = prev[1];
  return 4*samples;
}

int pcm_resample(short *dst, const short *src, int samples, int from_hz, int to_hz)
{
  if( from_hz < to_hz )
  {
    return pcm_resample_8k_32k(dst, src, samples);
  }
  else
    return 0;
}

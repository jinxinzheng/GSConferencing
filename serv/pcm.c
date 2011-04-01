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
    mix /= nbufs;

    if(mix > zero-1) mix = zero-1;
    else if(mix < -zero) mix = -zero;

    buf0[i] = (short)mix;
  }
}

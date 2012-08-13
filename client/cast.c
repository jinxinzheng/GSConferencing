#include  <stdlib.h>
#include  <stdio.h>
#include  "cast.h"
#include  "client.h"
#include  "pcm.h"
#include  "adpcm.h"
#include  "aac.h"

#define FRAMELEN  1024

static int compression;
static char tbuf[4096];
static char *send_buf;
static struct adpcm_state state;

void set_cast_mode(int mode)
{
  switch ( mode )
  {
    case MODE_BROADCAST : set_option(OPT_AUDIO_RBUDP_SEND, 1); break;
    case MODE_UNICAST :   set_option(OPT_AUDIO_SEND_UCAST, 1); break;
    case MODE_MULTICAST : set_option(OPT_AUDIO_MCAST_SEND, 1); break;
  }
}

int set_compression(int compr)
{
  compression = compr;
  if( compression == COMPR_AAC )
  {
    aacenc_init();
    return 4096;
  }
  else
    return FRAMELEN;
}

char *get_cast_buffer()
{
  send_buf = (char *)send_audio_start();
  switch( compression )
  {
    case COMPR_NONE:
    case COMPR_STEREO_TO_MONO:
      return send_buf;

    case COMPR_ADPCM:
    case COMPR_AAC:
      return tbuf;

    default:
      return NULL;
  }
}

int cast_filled(int len)
{
  switch( compression )
  {
    case COMPR_NONE:
      break;

    case COMPR_STEREO_TO_MONO:
      len = pcm_stereo_to_mono(send_buf, len);
      break;

    case COMPR_ADPCM:
      adpcm_coder((short *)tbuf, send_buf, len, &state);
      len = len/4;
      break;

    case COMPR_AAC:
      len = aac_encode(send_buf, 1024, tbuf, len);
      if( len<0 )
      {
        fprintf(stderr, "encode aac failed\n");
        return -1;
      }
      break;
  }
  send_audio_end(len);
  return 0;
}

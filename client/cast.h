#ifndef __CAST_H__
#define __CAST_H__


enum {
  MODE_BROADCAST,
  MODE_UNICAST,
  MODE_MULTICAST,
};

enum {
  COMPR_NONE,
  COMPR_STEREO_TO_MONO,
  COMPR_ADPCM,
  COMPR_AAC,
};

#define ADPCM_FRAMELEN  1024

void set_cast_mode(int mode);

/* returns the frame length required by the codec. */
int set_compression(int compr);

char *get_cast_buffer();

int cast_filled(int len);


#endif  /*__CAST_H__*/

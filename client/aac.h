#ifndef __AAC_H__
#define __AAC_H__


int aacenc_init();
int aac_encode(char *outbuf, int size, const char *src, int len);

int aacdec_init();
int aac_decode(char *outbuf, const char *inbuf, int len);


#endif  /*__AAC_H__*/

#ifndef __AAC_H__
#define __AAC_H__


typedef struct _aacenc aacenc_t;
aacenc_t *aacenc_init();
int aac_encode(aacenc_t *h_enc, char *outbuf, int size, const char *src, int len);

typedef struct _aacdec aacdec_t;
aacdec_t *aacdec_init();
int aac_decode(aacdec_t *h_dec, char *outbuf, const char *inbuf, int len);


#endif  /*__AAC_H__*/

#include <pvmp4audiodecoder_api.h>
#include <e_tmp4audioobjecttype.h>
#include <getactualaacconfig.h>

#include  <stdio.h>
#include  <stdint.h>
#include  <string.h>
#include  <stdlib.h>

#define KAAC_MAX_STREAMING_BUFFER_SIZE  (PVMP4AUDIODECODER_INBUFSIZE * 4)

typedef struct {
  tPVMP4AudioDecoderExternal *pExt;
  void *pMem;
  uint8_t *iInputBuf;
  int dec_sync;
  int dec_configed;
} aacdec_t;

aacdec_t *aacdec_init()
{
  aacdec_t *h_dec = (aacdec_t *)malloc(sizeof(aacdec_t));
  int memreq;

  h_dec->pExt = malloc(sizeof(tPVMP4AudioDecoderExternal));

  h_dec->iInputBuf = calloc(KAAC_MAX_STREAMING_BUFFER_SIZE, sizeof(uint8_t));

  memreq = PVMP4AudioDecoderGetMemRequirements();
  h_dec->pMem = malloc(memreq);

  //pExt->pOutputBuffer_plus = &iOutputBuf[2048];

  h_dec->pExt->pInputBuffer         = h_dec->iInputBuf;
  //h_dec->pExt->pOutputBuffer        = iOutputBuf;
  h_dec->pExt->inputBufferMaxLength     = KAAC_MAX_STREAMING_BUFFER_SIZE;
  h_dec->pExt->desiredChannels          = 2;
  h_dec->pExt->inputBufferCurrentLength = 0;
  h_dec->pExt->outputFormat             = OUTPUTFORMAT_16PCM_INTERLEAVED;
  h_dec->pExt->repositionFlag           = TRUE;
  h_dec->pExt->aacPlusEnabled           = TRUE;  /* Dynamically enable AAC+ decoding */
  h_dec->pExt->inputBufferUsedLength    = 0;
  h_dec->pExt->remainderBits            = 0;
  h_dec->pExt->frameLength              = 0;

  h_dec->pExt->inputBufferUsedLength=0;
  h_dec->pExt->inputBufferCurrentLength = 0;

  if (PVMP4AudioDecoderInitLibrary(h_dec->pExt, h_dec->pMem) != 0) {
    fprintf(stderr, "init library failed\n");
    free(h_dec);
    return NULL;
  }

  return h_dec;
}

static void append_inbuf(aacdec_t *h_dec, const char *inbuf, int len)
{
  int off = h_dec->pExt->inputBufferUsedLength;
  int exists = h_dec->pExt->inputBufferCurrentLength - h_dec->pExt->inputBufferUsedLength;
  if( exists <= 0 )
  {
    h_dec->pExt->inputBufferUsedLength = 0;
    h_dec->pExt->inputBufferCurrentLength = 0;
    off = 0;
    exists = 0;
  }
  if( off > 1024 )
  {
    memmove(h_dec->pExt->pInputBuffer,
        h_dec->pExt->pInputBuffer+off,
        exists);
    h_dec->pExt->inputBufferUsedLength = 0;
    h_dec->pExt->inputBufferCurrentLength = exists;
  }
  memcpy(h_dec->pExt->pInputBuffer + h_dec->pExt->inputBufferCurrentLength,
      inbuf, len);
  h_dec->pExt->inputBufferCurrentLength += len;
}

int aac_decode(aacdec_t *h_dec, char *outbuf, const char *inbuf, int len)
{
  int Status;

  /* seek to first adts sync */
  if( !h_dec->dec_sync )
  {
    uint8_t *inp = (uint8_t *)inbuf;
    if( (inp[0] == 0xFF) && ((inp[1] & 0xF6) == 0xF0) )
    {
      h_dec->dec_sync = 1;
    }
    else
      return 0;
  }

  append_inbuf(h_dec, inbuf, len);

  /* needed for some stream encoded on neon. */
  if( h_dec->pExt->inputBufferCurrentLength - h_dec->pExt->inputBufferUsedLength < 256 )
  {
    return 0;
  }

  //pExt->pInputBuffer         = (uint8_t *)inbuf;
  h_dec->pExt->pOutputBuffer        = (short *)outbuf;
  h_dec->pExt->pOutputBuffer_plus   = (short *)(outbuf+4096);

  //pExt->inputBufferUsedLength=0;
  //pExt->inputBufferCurrentLength = len;

  if( !h_dec->dec_configed )
  {
    Status = PVMP4AudioDecoderConfig(h_dec->pExt, h_dec->pMem);
    /*fprintf(stderr, "config %d. used %d, current %d\n",
        Status,
        pExt->inputBufferUsedLength,
        pExt->inputBufferCurrentLength);*/
  }

  Status = PVMP4AudioDecodeFrame(h_dec->pExt, h_dec->pMem);

  if (MP4AUDEC_SUCCESS == Status || SUCCESS == Status) {
    /*fprintf(stderr, "[SUCCESS] Status: SUCCESS "
                  "inputBufferUsedLength: %u, "
                  "inputBufferCurrentLength: %u, "
                  "remainderBits: %u, "
                  "frameLength: %u\n",
                  pExt->inputBufferUsedLength,
                  pExt->inputBufferCurrentLength,
                  pExt->remainderBits,
                  pExt->frameLength);*/
    if( h_dec->pExt->frameLength > 0 && !h_dec->dec_configed )
    {
      h_dec->dec_configed = 1;
    }
  }
  else
  {
    fprintf(stderr, "aac decode error %d\n", Status);
    fprintf(stderr, "used %d, current %d\n",
        h_dec->pExt->inputBufferUsedLength,
        h_dec->pExt->inputBufferCurrentLength);
#if 1
    if( h_dec->pExt->inputBufferUsedLength > h_dec->pExt->inputBufferCurrentLength )
    {
      /* this is ridiculous, but does happen..
       * try to re-init the decoder. */
      h_dec->dec_sync = 0;
      h_dec->dec_configed = 0;
      h_dec->pExt->inputBufferUsedLength=0;
      h_dec->pExt->inputBufferCurrentLength = 0;
      PVMP4AudioDecoderResetBuffer(h_dec->pMem);
      PVMP4AudioDecoderInitLibrary(h_dec->pExt, h_dec->pMem);
    }
#endif
    return -1;
  }

  return h_dec->pExt->frameLength*4;
}

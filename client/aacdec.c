#include "opencore-aacdec/include/pvmp4audiodecoder_api.h"
#include "opencore-aacdec/include/e_tmp4audioobjecttype.h"
#include "opencore-aacdec/include/getactualaacconfig.h"

#include  <stdio.h>
#include  <stdint.h>
#include  <string.h>
#include  <stdlib.h>

#define KAAC_MAX_STREAMING_BUFFER_SIZE  (PVMP4AUDIODECODER_INBUFSIZE * 4)

static tPVMP4AudioDecoderExternal *pExt;
static void *pMem;
static uint8_t *iInputBuf;
static int dec_sync;
static int dec_configed;

int aacdec_init()
{
  int memreq;

  pExt = malloc(sizeof(tPVMP4AudioDecoderExternal));

  iInputBuf = calloc(KAAC_MAX_STREAMING_BUFFER_SIZE, sizeof(uint8_t));

  memreq = PVMP4AudioDecoderGetMemRequirements();
  pMem = malloc(memreq);

  //pExt->pOutputBuffer_plus = &iOutputBuf[2048];

  pExt->pInputBuffer         = iInputBuf;
  //pExt->pOutputBuffer        = iOutputBuf;
  pExt->inputBufferMaxLength     = KAAC_MAX_STREAMING_BUFFER_SIZE;
  pExt->desiredChannels          = 2;
  pExt->inputBufferCurrentLength = 0;
  pExt->outputFormat             = OUTPUTFORMAT_16PCM_INTERLEAVED;
  pExt->repositionFlag           = TRUE;
  pExt->aacPlusEnabled           = TRUE;  /* Dynamically enable AAC+ decoding */
  pExt->inputBufferUsedLength    = 0;
  pExt->remainderBits            = 0;
  pExt->frameLength              = 0;

  pExt->inputBufferUsedLength=0;
  pExt->inputBufferCurrentLength = 0;

  if (PVMP4AudioDecoderInitLibrary(pExt, pMem) != 0) {
    fprintf(stderr, "init library failed\n");
    return -1;
  }

  return 0;
}

static void append_inbuf(const char *inbuf, int len)
{
  int off = pExt->inputBufferUsedLength;
  int exists = pExt->inputBufferCurrentLength - pExt->inputBufferUsedLength;
  if( exists <= 0 )
  {
    pExt->inputBufferUsedLength = 0;
    pExt->inputBufferCurrentLength = 0;
    off = 0;
    exists = 0;
  }
  if( off > 1024 )
  {
    memmove(pExt->pInputBuffer,
        pExt->pInputBuffer+off,
        exists);
    pExt->inputBufferUsedLength = 0;
    pExt->inputBufferCurrentLength = exists;
  }
  memcpy(pExt->pInputBuffer + pExt->inputBufferCurrentLength,
      inbuf, len);
  pExt->inputBufferCurrentLength += len;
}

int aac_decode(char *outbuf, const char *inbuf, int len)
{
  int Status;

  /* seek to first adts sync */
  if( !dec_sync )
  {
    uint8_t *inp = (uint8_t *)inbuf;
    if( (inp[0] == 0xFF) && ((inp[1] & 0xF6) == 0xF0) )
    {
      dec_sync = 1;
    }
    else
      return 0;
  }

  append_inbuf(inbuf, len);

  /* needed for some stream encoded on neon. */
  if( pExt->inputBufferCurrentLength - pExt->inputBufferUsedLength < 256 )
  {
    return 0;
  }

  //pExt->pInputBuffer         = (uint8_t *)inbuf;
  pExt->pOutputBuffer        = (short *)outbuf;
  pExt->pOutputBuffer_plus   = (short *)(outbuf+4096);

  //pExt->inputBufferUsedLength=0;
  //pExt->inputBufferCurrentLength = len;

  if( !dec_configed )
  {
    Status = PVMP4AudioDecoderConfig(pExt, pMem);
    /*fprintf(stderr, "config %d. used %d, current %d\n",
        Status,
        pExt->inputBufferUsedLength,
        pExt->inputBufferCurrentLength);*/
  }

  Status = PVMP4AudioDecodeFrame(pExt, pMem);

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
    if( pExt->frameLength > 0 && !dec_configed )
    {
      dec_configed = 1;
    }
  }
  else
  {
    fprintf(stderr, "aac decode error %d\n", Status);
    fprintf(stderr, "used %d, current %d\n",
        pExt->inputBufferUsedLength,
        pExt->inputBufferCurrentLength);
#if 1
    if( pExt->inputBufferUsedLength > pExt->inputBufferCurrentLength )
    {
      /* this is ridiculous, but does happen..
       * try to re-init the decoder. */
      dec_sync = 0;
      dec_configed = 0;
      pExt->inputBufferUsedLength=0;
      pExt->inputBufferCurrentLength = 0;
      PVMP4AudioDecoderResetBuffer(pMem);
      PVMP4AudioDecoderInitLibrary(pExt, pMem);
    }
#endif
    return -1;
  }

  return pExt->frameLength*4;
}

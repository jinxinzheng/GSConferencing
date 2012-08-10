#include "vo-aacenc/include/voAAC.h"
#include "vo-aacenc/include/cmnMemory.h"
#include "opencore-aacdec/include/pvmp4audiodecoder_api.h"
#include "opencore-aacdec/include/e_tmp4audioobjecttype.h"
#include "opencore-aacdec/include/getactualaacconfig.h"

#include  <stdio.h>
#include  <stdint.h>
#include  <string.h>
#include  <stdlib.h>

static int sampleRate = 44100;
static int channels = 2;
static int bitrate = 64000;
static VO_AUDIO_CODECAPI codec_api;
static VO_HANDLE handle;
static VO_MEM_OPERATOR mem_operator;
static VO_CODEC_INIT_USERDATA user_data;
static AACENC_PARAM params;

int aacenc_init()
{
  voGetAACEncAPI(&codec_api);

  mem_operator.Alloc = cmnMemAlloc;
  mem_operator.Copy = cmnMemCopy;
  mem_operator.Free = cmnMemFree;
  mem_operator.Set = cmnMemSet;
  mem_operator.Check = cmnMemCheck;
  user_data.memflag = VO_IMF_USERMEMOPERATOR;
  user_data.memData = &mem_operator;
  codec_api.Init(&handle, VO_AUDIO_CodingAAC, &user_data);

  params.sampleRate = sampleRate;
  params.bitRate = bitrate;
  params.nChannels = channels;
  params.adtsUsed = 1;
  if (codec_api.SetParam(handle, VO_PID_AAC_ENCPARAM, &params) != VO_ERR_NONE) {
    fprintf(stderr, "Unable to set encoding parameters\n");
    return -1;
  }
  return 0;
}

int aac_encode(char *outbuf, int size, const char *src, int len)
{
  VO_CODECBUFFER input, output;
  VO_AUDIO_OUTPUTINFO output_info;
  memset(&input, 0, sizeof(input));
  memset(&output, 0, sizeof(output));
  memset(&output_info, 0, sizeof(output_info));

  input.Buffer = (uint8_t*) src;
  input.Length = len;
  codec_api.SetInputData(handle, &input);

  output.Buffer = (uint8_t *)outbuf;
  output.Length = size;
  if (codec_api.GetOutputData(handle, &output, &output_info) != VO_ERR_NONE) {
    fprintf(stderr, "Unable to encode frame\n");
    return -1;
  }
  return output.Length;
}


#define KAAC_MAX_STREAMING_BUFFER_SIZE  (PVMP4AUDIODECODER_INBUFSIZE * 4)

static tPVMP4AudioDecoderExternal *pExt;
static void *pMem;
static uint8_t *iInputBuf;
static int dec_sync;

int aacdec_init()
{
  int memreq;
  int Status;

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
  if( exists > 0 )
  {
    memmove(pExt->pInputBuffer,
        pExt->pInputBuffer+off,
        exists);
  }
  memcpy(pExt->pInputBuffer+exists, inbuf, len);
  pExt->inputBufferUsedLength = 0;
  pExt->inputBufferCurrentLength = exists+len;
}

int aac_decode(char *outbuf, const char *inbuf, int len)
{
  int Status;

  //pExt->pInputBuffer         = (uint8_t *)inbuf;
  pExt->pOutputBuffer        = (short *)outbuf;
  pExt->pOutputBuffer_plus   = (short *)outbuf+4096;

  append_inbuf(inbuf, len);
  //pExt->inputBufferUsedLength=0;
  //pExt->inputBufferCurrentLength = len;

  if( !dec_sync )
  {
    Status = PVMP4AudioDecoderConfig(pExt, pMem);
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
    if( pExt->frameLength > 0 && !dec_sync )
    {
      dec_sync = 1;
    }
  }
  else
  {
    fprintf(stderr, "aac decode error %d\n", Status);
    return -1;
  }

  return pExt->frameLength*4;
}

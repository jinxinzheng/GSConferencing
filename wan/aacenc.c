#include <voAAC.h>
#include <cmnMemory.h>

#include  <stdio.h>
#include  <stdint.h>
#include  <string.h>
#include  <stdlib.h>

typedef struct {
  int sampleRate;
  int channels;
  int bitrate;
  VO_AUDIO_CODECAPI codec_api;
  VO_HANDLE handle;
  VO_MEM_OPERATOR mem_operator;
  VO_CODEC_INIT_USERDATA user_data;
  AACENC_PARAM params;
} aacenc_t;

aacenc_t *aacenc_init()
{
  aacenc_t *h_enc = malloc(sizeof(aacenc_t));

  h_enc->sampleRate = 44100;
  h_enc->channels = 2;
  h_enc->bitrate = 64000;

  voGetAACEncAPI(&h_enc->codec_api);

  h_enc->mem_operator.Alloc = cmnMemAlloc;
  h_enc->mem_operator.Copy = cmnMemCopy;
  h_enc->mem_operator.Free = cmnMemFree;
  h_enc->mem_operator.Set = cmnMemSet;
  h_enc->mem_operator.Check = cmnMemCheck;
  h_enc->user_data.memflag = VO_IMF_USERMEMOPERATOR;
  h_enc->user_data.memData = &h_enc->mem_operator;
  h_enc->codec_api.Init(&h_enc->handle, VO_AUDIO_CodingAAC, &h_enc->user_data);

  h_enc->params.sampleRate = h_enc->sampleRate;
  h_enc->params.bitRate = h_enc->bitrate;
  h_enc->params.nChannels = h_enc->channels;
  h_enc->params.adtsUsed = 1;
  if (h_enc->codec_api.SetParam(h_enc->handle, VO_PID_AAC_ENCPARAM, &h_enc->params) != VO_ERR_NONE) {
    fprintf(stderr, "Unable to set encoding parameters\n");
    free(h_enc);
    return NULL;
  }
  return h_enc;
}

int aac_encode(aacenc_t *h_enc, char *outbuf, int size, const char *src, int len)
{
  VO_CODECBUFFER input, output;
  VO_AUDIO_OUTPUTINFO output_info;
  memset(&input, 0, sizeof(input));
  memset(&output, 0, sizeof(output));
  memset(&output_info, 0, sizeof(output_info));

  input.Buffer = (uint8_t*) src;
  input.Length = len;
  h_enc->codec_api.SetInputData(h_enc->handle, &input);

  output.Buffer = (uint8_t *)outbuf;
  output.Length = size;
  if (h_enc->codec_api.GetOutputData(h_enc->handle, &output, &output_info) != VO_ERR_NONE) {
    fprintf(stderr, "Unable to encode frame\n");
    return -1;
  }
  return output.Length;
}

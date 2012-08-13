#include "vo-aacenc/include/voAAC.h"
#include "vo-aacenc/include/cmnMemory.h"

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

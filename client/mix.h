#ifndef __MIX_H__
#define __MIX_H__


void mix_audio_init();
/* set auto closing empty queues when the queued
 * packs hits the max limit. */
void mix_audio_auto_close(int max_packs);

void mix_audio_open(int dev_id);
void mix_audio_close(int dev_id);

int put_mix_audio(struct pack *pack);
struct pack *get_mix_audio();


#endif  /*__MIX_H__*/

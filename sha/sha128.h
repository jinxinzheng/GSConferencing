/*--------------------------   SHA Algorithsm   ---------------------------*/
/*--------------------------  Header File Sha.h ---------------------------*/
#ifndef _SHA_H_
#define _SHA_H_

#include "globaldt.h"

#ifdef __cplusplus
extern "C"{
#endif

//注意定义 LITTLE_ENDIAN 表示字节序
#define SHA_DIGESTSIZE 16

//The SHA block size and message digest sizes,in bytes
#define SHA_BLOCKSIZE  64

//The structure for store SHA info
typedef struct
{
	DWORD digest[ 5 ];     //message digest
	DWORD countLo,countHi; //64-bit bit count
	DWORD data[16];        //SHA data buffer
}SHA_INFO;

void shaHashData( BYTE * data, int len, BYTE * res );
void shaInit( SHA_INFO *shaInfo );
void shaUpdate( SHA_INFO *shaInfo, BYTE *buffer, int count );
BYTE* shaFinal( SHA_INFO *shaInfo );

#ifdef __cplusplus
}
#endif

#endif

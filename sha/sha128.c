#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "sha128.h"

//Whether machine is little-endian or not
#define LITTLE_ENDIAN

//THE SHA f() FUNCTION

#define f1(x,y,z) ( ( x&y ) | ( ~x&z ) )
#define f2(x,y,z) ( ( x&y ) | ( x&~z ) | ( y&~z ) )
#define f3(x,y,z) ( ( x&~z ) | ( y&z ) )
#define f4(x,y,z) ( ( x&y ) | ( x&z ) | ( y&z ) )

//The SHA Initial Value
#define h0init 0x724d65f1L
#define h1init 0xec38a0b9L
#define h2init 0xd9de387fL
#define h3init 0x5c1602a4L

//32-bit rotate-kludged with shifts
#define S(n,x) ( (x<<n) | ( x>>( 32-n ) ) )

//The initial expanding function
#define expand( count ) \
	{   \
		W[count]=W[count-4 ]^W[count-5]^W[count-13]^W[count-16];\
	}

// The four SHA sub_rounds

#define SubRound1( count ) 	\
	{\
	  temp = S( 5,A ) + f1( B,C,W[count] ) + S( 7,D ) + K[count]; \
	  D    = C;\
	  C	   = S( 30,B );\
	  B    = A;\
	  A    = temp;\
	}

#define SubRound2( count ) \
	{ \
	  temp = S( 5,A ) + f2( B,C,W[count] ) + S( 7,D ) + K[count]; \
	  D    = C;\
	  C	   = S( 30,B );\
	  B    = A;\
	  A    = temp;\
	}

#define SubRound3( count ) \
	{ \
	  temp = S( 5,A ) + f3( B,C,W[count] ) + S( 7,D ) + K[count]; \
	  D    = C;\
	  C	   = S( 30,B );\
	  B    = A;\
	  A    = temp;\
	}

#define SubRound4( count ) \
	{ \
	  temp = S( 5,A ) + f4( B,C,W[count] ) + S( 7,D ) + K[count]; \
	  D    = C;\
	  C	   = S( 30,B );\
	  B    = A;\
	  A    = temp;\
	}


DWORD h0,h1,h2,h3,h4;
DWORD A,B,C,D,E;
DWORD K[80]=
	{	0xE8C7B756 ,0xC1BDCEEE ,0x4787C62A ,0xFD469501 ,0x8B44F7AF ,0x895CD7BE ,0xFD987193 ,
		0x49B40821 ,0xC040B340 ,0xE9B6C7AA,0x02441453 ,0xE7D3FBC8 ,0xC33707D6 ,0x455A14ED ,
		0xFCEFA3F8 ,0x8D2A4C8A ,0x8771F681 ,0xFDE5380C ,0x4BDECFA9 ,0xBEBFBC70 ,0xEAA127FA ,
		0x04881D05 ,0xE6DB99E5 ,0xC4AC5665 ,0x432AFF97 ,0xFC93A039 ,0x8F0CCC92 ,0x85845DD1 ,
		0xFE2CE6E0 ,0x4E0811A1 ,0xBD3AF235 ,0xEB86D391 ,0x06CC0E72 ,0xE5DE96A6 ,0xC61DB31F ,
		0x40FA9162 ,0xFC328B9A ,0x90EC6E1A ,0x83941793 ,0xFE6F7CA0 ,0x502FC2F0 ,0xBBB25C5E ,
		0xEC67C5D5 ,0x90FDCF8 ,0xE4DCF720 ,0xC78B169D ,0x3EC8D588 ,0xFBCC680F ,0x92C92784 ,
		0x81A12DBC ,0xFEACF7F6 ,0x5255D887 ,0xBA2602C9 ,0xED43FA42 ,0xB537CF7 ,0xE3D6C07E ,
		0xC8F47989 ,0x3C95D750 ,0xFB6137A4 ,0x94A2EF3F ,0x7FABAA4E ,0xFEE557A5 ,0x547A475C ,
		0xB895ED69 ,0xEE1B6C6D ,0xD96E2CE ,0xE2CBF801 ,0xCA59D4A4 ,0x3A61A204 ,0xFAF0FC7F ,
		0x9679BBCC ,0x7DB39757 ,0xFF189A8E ,0x569D046F ,0xB7022445 ,0xEEEE1806 ,0xFDA02DF ,
		0xE1BCA303 ,0xCBBB20C3 ,0x382C40F4 };
//initialize the SHA values

void shaInit( SHA_INFO *shaInfo )
{
	//set the h_values to their initial values
	shaInfo->digest[ 0 ] = h0init;
	shaInfo->digest[ 1 ] = h1init;
	shaInfo->digest[ 2 ] = h2init;
	shaInfo->digest[ 3 ] = h3init;

	//initialise bit count
	shaInfo->countLo = 0L;
	shaInfo->countHi = 0L;

	memset( (BYTE *)&shaInfo->data,0,64);
 }

 //Perform The SHA Tranform. For accelate the speed, not use cycle structer

void shaTranform( SHA_INFO *shaInfo )
 {
	DWORD W[ 80 ],temp;
	int i;

	//Step 1, copy data to local work area
	for (i = 0;i<16;i++)
		W[ i ] = shaInfo->data[ i ];

	expand( 16 );expand( 17 );expand( 18 );expand( 19 );
	expand( 20 );expand( 21 );expand( 22 );expand( 23 );
	expand( 24 );expand( 25 );expand( 26 );expand( 27 );
	expand( 28 );expand( 29 );expand( 30 );expand( 31 );
	expand( 32 );expand( 33 );expand( 34 );expand( 35 );
	expand( 36 );expand( 37 );expand( 38 );expand( 39 );
	expand( 40 );expand( 41 );expand( 42 );expand( 43 );
	expand( 44 );expand( 45 );expand( 46 );expand( 47 );
	expand( 48 );expand( 49 );expand( 50 );expand( 51 );
	expand( 52 );expand( 53 );expand( 54 );expand( 55 );
	expand( 56 );expand( 57 );expand( 58 );expand( 59 );
	expand( 60 );expand( 61 );expand( 62 );expand( 63 );
	expand( 64 );expand( 65 );expand( 66 );expand( 67 );
	expand( 68 );expand( 69 );expand( 70 );expand( 71 );
	expand( 72 );expand( 73 );expand( 74 );expand( 75 );
	expand( 76 );expand( 77 );expand( 78 );expand( 79 );

	//Step 3, set up first buffer
	A = shaInfo->digest[0];
	B = shaInfo->digest[1];
	C = shaInfo->digest[2];
	D = shaInfo->digest[3];

	//Step 4, Serious mangling , divided into four SubRound

	SubRound1  (0);	SubRound1  (1);	SubRound1  (2);	SubRound1  (3);
	SubRound1  (4); SubRound1  (5); SubRound1  (6); SubRound1  (7);
	SubRound1  (8); SubRound1  (9); SubRound1 (10); SubRound1 (11);
	SubRound1 (12); SubRound1 (13); SubRound1 (14); SubRound1 (15);
	SubRound1 (16); SubRound1 (17); SubRound1 (18); SubRound1 (19);

	SubRound2 (20); SubRound2 (21); SubRound2 (22); SubRound2 (23);
	SubRound2 (24); SubRound2 (25); SubRound2 (26); SubRound2 (27);
	SubRound2 (28); SubRound2 (29); SubRound2 (30); SubRound2 (31);
	SubRound2 (32); SubRound2 (33); SubRound2 (34); SubRound2 (35);
	SubRound2 (36); SubRound2 (37); SubRound2 (38); SubRound2 (39);

	SubRound3 (40); SubRound3 (41); SubRound3 (42); SubRound3 (43);
	SubRound3 (44); SubRound3 (45); SubRound3 (46); SubRound3 (47);
	SubRound3 (48); SubRound3 (49); SubRound3 (50); SubRound3 (51);
	SubRound3 (52); SubRound3 (53); SubRound3 (54); SubRound3 (55);
	SubRound3 (56); SubRound3 (57); SubRound3 (58); SubRound3 (59);

	SubRound4 (60); SubRound4 (61); SubRound4 (62); SubRound4 (63);
	SubRound4 (64); SubRound4 (65); SubRound4 (66); SubRound4 (67);
	SubRound4 (68); SubRound4 (69); SubRound4 (70); SubRound4 (71);
	SubRound4 (72); SubRound4 (73); SubRound4 (74); SubRound4 (75);
	SubRound4 (76); SubRound4 (77); SubRound4 (78); SubRound4 (79);

	//Step 5, build message digest
	shaInfo->digest[0] += A;
	shaInfo->digest[1] += B;
	shaInfo->digest[2] += C;
	shaInfo->digest[3] += D;

#ifdef DEBUG
//	all4Test( shaInfo );
#endif

   }

#ifdef LITTLE_ENDIAN

/*When run on a little_endian CPU we need to perform bytes reversal on an
  array of longwords. It is possible to make the code endianness-independent
  by fiddling around with data at the byte level,but this makes for very slow
  code. So we rely on the user to sort out endianness at compile time.*/

static void byteReverse( DWORD * buffer,int byteCount )
{
	DWORD value;
	int  count;

	byteCount /= sizeof( DWORD );
	for ( count = 0; count < byteCount; count++ )
		{
		 value = ( buffer[ count ] << 16 | buffer[ count ] >> 16 );
		 buffer[ count ] = ( ( value & 0xFF00FF00L ) >> 8 ) |
						   ( ( value & 0x00FF00FFL ) << 8 );
		}

 }

 #endif //LITTLE_ENDIAN

 /*Update SHA for a block of data. This code assumes that the buffer size
 is a multiple of SHA_BLOCKSIZE bytes long. which makes the code a lot more
 efficient since it does away with the need to handle partial blocks
 between calls to shaUpdate( )*/

void shaUpdate( SHA_INFO *shaInfo, BYTE *buffer, int count )
 {
	//Update bitcount
	if (( shaInfo->countLo + ( (DWORD) count<<3 ) ) < shaInfo->countLo )
		shaInfo->countHi ++; //Carry from low to high bitCount

	shaInfo->countLo += ( (DWORD) count << 3  );
	shaInfo->countHi += ( (DWORD) count >> 29 );

	//Process data in SHA_BLOCKSIZE chunks

	while ( count >= SHA_BLOCKSIZE )
		{
			memcpy( shaInfo->data,buffer,SHA_BLOCKSIZE );
#ifdef LITTLE_ENDIAN
			byteReverse( shaInfo->data,SHA_BLOCKSIZE );
#endif
			shaTranform( shaInfo );
			buffer += SHA_BLOCKSIZE;
			count  -= SHA_BLOCKSIZE;
		 }
	//Handle any remaining bytes of data. This should only happen once on
	//the final lot of data
	memcpy( shaInfo->data,buffer,count ) ;
}

BYTE* shaFinal( SHA_INFO *shaInfo )
{
	int count;
	DWORD lowBitcount = shaInfo->countLo;
	DWORD highBitcount = shaInfo->countHi;

	// Computer number of bytes mod 64
	count = (int) (( shaInfo->countLo >> 3) & 0x3f);

	/* Set the first char of padding to 0x80. This is safe since there is
	  always at least one byte free*/

   ( ( BYTE* ) shaInfo->data )[ count ++ ] = 0x80;

   //Pad out to 56 mod 64

   if ( count > 56 )
	{
		//Two lots of padding; Pad the first block to 64 bytes;
		memset( (BYTE *)&shaInfo->data+count,0,64-count);
#ifdef LITTLE_ENDIAN
		byteReverse( shaInfo->data,SHA_BLOCKSIZE );
#endif
		shaTranform( shaInfo );

		//Now fill the next block with 56 bytes
		memset( (BYTE *)&shaInfo->data,0,56);
	}
	else
	{
		memset( (BYTE * )&shaInfo->data + count,0,56-count);
	}

#ifdef LITTLE_ENDIAN
	byteReverse( shaInfo->data,SHA_BLOCKSIZE);
#endif

   shaInfo->data[ 14 ] = highBitcount;
   shaInfo->data[ 15 ] = lowBitcount;

//Append length in bits and transform

	shaTranform( shaInfo );
#ifdef LITTLE_ENDIAN
	byteReverse( shaInfo->digest,SHA_DIGESTSIZE ) ;
#endif
	return (BYTE*)shaInfo->digest;
}

void shaHashData( BYTE * data, int len, BYTE * res )
{
	SHA_INFO sha_Info;

	shaInit(&sha_Info);
	shaUpdate(&sha_Info, data, len);
	shaFinal( &sha_Info );
	memcpy( res,sha_Info.digest,16);
}


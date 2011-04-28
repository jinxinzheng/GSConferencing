#include "sha/sha128.h"

//#define h(t) (t<0xa? '0'+t : 'a'+(t-0xa))
#define h(t) (t<0xa? '#'+t : ':'+(t-0xa))
//#define h(t) ('!'+t)

static inline void i2a(unsigned char i, char a[2])
{
  int t;
  t = i>>4;
  a[0] = h(t);
  t = i%16;
  a[1] = h(t);
}

static inline void cksum(void *data, int len, char *sum)
{
  unsigned char salt[16] = {
    0x29, 0x55, 0xba, 0xb3, 0x9e, 0xbc, 0xe4, 0xf3,
    0x5b, 0x71, 0x12, 0x45, 0xe8, 0xc6, 0x6f, 0xd5,
  };
  unsigned int *pu = (unsigned int *)salt;
  unsigned int *ps = (unsigned int *)sum;
  int i;

  shaHashData(data, len, (unsigned char *)sum);

  for (i=0; i<sizeof(salt)/sizeof(int); i++)
  {
    ps[i] ^= pu[i];
  }

  for (i=16-1; i>=0; i--)
  {
    i2a(((unsigned char *)sum)[i], sum+i*2);
    //sum[i] = '!' + ((unsigned char *)sum)[i]%('~'-'!');
  }

  sum[32] = 0;
}

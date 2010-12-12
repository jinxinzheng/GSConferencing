#include "include/cksum.h"

int main(int argc, char *argv[])
{
  int id, type;
  unsigned int i;
  char s[64];
  if (argc<3)
  {
    fprintf(stderr, "%s id type\n", argv[0]);
    return 1;
  }
  id = atoi(argv[1]);
  type = atoi(argv[2]);
  i = (unsigned int)id ^ type;
  cksum(&i, sizeof i, s);
  printf("%s\n", s);
}

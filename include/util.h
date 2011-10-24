#ifndef _UTIL_H_
#define _UTIL_H_

#define die(s) do {perror(s); exit(1);} while(0)
#define fail(s) do {perror(s); return -1;} while(0)

#define offsetof(type, memb)  (size_t)&(((type *)0)->memb)

#define array_size(arr)   (sizeof(arr) / sizeof((arr)[0]))

#endif

#ifndef _UTIL_H_
#define _UTIL_H_

#define msleep(i) usleep((i)*1000)

#define die(s) do {perror(s); exit(1);} while(0)
#define fail(s) do {perror(s); return -1;} while(0)

#define offsetof(type, memb)  (ssize_t)&(((type *)0)->memb)

#define array_size(arr)   ((int)(sizeof(arr) / sizeof((arr)[0])))

#endif

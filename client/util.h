#ifndef _UTIL_H_
#define _UTIL_H_

#define die(s) do {perror(s); exit(1);} while(0)
#define fail(s) do {perror(s); return -1;} while(0)

#endif

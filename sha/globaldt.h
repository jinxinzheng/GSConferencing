/*@@@
File:			gdt.h
Description:	Generic Data Type definitions
为了移植需要，将常用的数据类型统一在此处重新定义
@@@*/

#ifndef GDT_H
#define GDT_H

//#ifdef Old_Style
#include <stdio.h>
//#else
//#include <cstdio>
//#endif

#define SIZEOF_SHORT_INT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG_INT 4
#define SIZEOF_LONG_LONG_INT 8

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;
typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

/* System-specific typedefs */

/* INT2 */
#if (SIZEOF_INT == 2)
	typedef int INT2;
	typedef unsigned int UINT2;
#else
#if (SIZEOF_SHORT_INT == 2)
	typedef short int INT2;
	typedef unsigned short UINT2;
#endif
#endif
	
typedef UINT2 WCHR;		//wchar_t，宽字符，用于Unicode
typedef UINT2* PUINT2;

/* INT4 */
#if (SIZEOF_INT == 4)
	typedef int INT4;
	typedef unsigned int UINT4;
#else
#if (SIZEOF_LONG_INT == 4)
	typedef long int INT4;
	typedef unsigned long int UINT4;
#endif
#endif

#endif /* GDT_H */

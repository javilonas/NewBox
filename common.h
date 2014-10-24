#ifndef _COMMON_H_
#define _COMMON_H_

#define FD_SETSIZE2  4196 // 4096 default

#define DELAY_MAIN	10000
#define DELAY_RECV	0
#define DELAY_ACCEPT	100000


#ifdef CCCAM_SRV
#define CCCAM
#else
#ifdef CCCAM_CLI
#define CCCAM
#endif
#endif

#ifdef RADEGAST_SRV
#define RADEGAST
#else
#ifdef RADEGAST_CLI
#define RADEGAST
#endif
#endif

#define DATE_BUILD    "24-10-2014"
#define VERSION    "01"
#define REVISION   01

#define FALSE 0
#define TRUE 1

typedef unsigned char uchar;
typedef unsigned short int usint;
//typedef unsigned int uint;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;


// win32 definitions
#ifndef WIN32

typedef int SOCKET;
typedef int HANDLE;
#define INVALID_HANDLE_VALUE -1
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#endif

#endif

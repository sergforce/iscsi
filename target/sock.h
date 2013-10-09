#ifndef _SOCK_H
#define _SOCK_H


#ifdef WIN32
#include <winsock2.h>
#define LITTLE_ENDIAN

typedef unsigned __int64	uint64_t;
typedef unsigned int		uint32_t;
typedef unsigned short		uint16_t;
typedef unsigned char		uint8_t;

typedef __int64		int64_t;
typedef int			int32_t;
typedef short		int16_t;
typedef char		int8_t;

#define LONG_FORMAT "%I64d"

#define snprintf _snprintf
#else

#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int SOCKET;
#define LONG_FORMAT "%ji"

#endif

#ifdef _DEBUG2
#include <stdio.h>
#endif

int readWords(SOCKET s, uint32_t *ptr, int bytes2read, int timeout);
int readBytes(SOCKET s, char *ptr, int bytes2read, int timeout);

int writeWords(SOCKET s, uint32_t *ptr, int bytes2write, int timeout);
int writeBytes(SOCKET s, char *ptr, int bytes2write, int timeout);

/* Disable command queue */
#define _SYNC_SCSI 



#endif

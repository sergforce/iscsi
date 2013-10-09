#ifndef __BUFFER_H_
#define __BUFFER_H_

typedef unsigned __int64	uint64_t;
typedef unsigned int		uint32_t;
typedef unsigned short		uint16_t;
typedef unsigned char		uint8_t;

struct DCirList {
	struct DCirList *next; 
	struct DCirList *prev; 
};


struct Buffer {
	struct DCirList list;
	struct BufferAllocator *belong;
	uint32_t maxLength;
	uint32_t length;
	uint8_t data[1];
};

struct AllocParam {
	uint32_t maxLength;
	uint8_t  nIncrease;
	uint8_t  nDecrease;
	uint16_t nIntialCount;
};

struct BufferAllocator {
	struct DCirList list;
	struct Buffer *buffFree;
	struct Buffer *buffUsed;

	uint32_t useds;
	uint32_t frees;

	struct BufferAllocator *parnetAllocator;
	int (*parentLock)(struct BufferAllocator *parentAlloc);
	int (*parentFree)(struct BufferAllocator *parentAlloc);
	
	struct AllocParam ap;
};

#endif
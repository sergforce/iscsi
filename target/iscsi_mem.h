#ifndef __ISCSI_MEM_H_
#define __ISCSI_MEM_H_

struct _Buffer {
	struct DCirList list;
	struct BufferAllocator *belong;
	uint32_t maxLength;
	uint32_t length;
	uint8_t _data[44];
};

struct Buffer {
	struct DCirList list;
	struct BufferAllocator *belong;
	uint32_t maxLength;
	uint32_t length;
	uint8_t _data[44];
	uint8_t data[1];
};

/*
struct _Buffer {
	struct DCirList list;
	struct BufferAllocator *belong;
	uint32_t maxLength;
	uint32_t length;
};

struct Buffer {
	struct DCirList list;
	struct BufferAllocator *belong;
	uint32_t maxLength;
	uint32_t length;
	uint8_t data[12];
};
*/
struct AllocParam {
	uint32_t maxLength;
	uint8_t  nIncrease;
	uint8_t  nDecrease;
	uint8_t  nIntialCount;
	uint8_t  policy; /* 0 dynamic buffers, 1 static buffers */

	/*uint32_t nMaxCount;*/ 
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

struct Buffer *allocLocalBuff(struct BufferAllocator *ba, uint32_t size);
int AllocBuffers(struct BufferAllocator *ba);
int freeMemoryBuff(struct BufferAllocator *ba);


int bufferAllocatorInit(struct BufferAllocator *ba, struct AllocParam *prm, uint32_t sizeCounts);
struct BufferAllocator *bufferAllocatorCreate(struct AllocParam *prm, uint32_t sizeCounts);
struct Buffer *allocBuff(struct BufferAllocator *ba, uint32_t size);
int freeBuff(struct Buffer *buff);
void bufferAllocatorClean(struct BufferAllocator *ba);


#endif

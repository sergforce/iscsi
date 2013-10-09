#include "sock.h"
#include "dcirlist.h"
#include "iscsi_mem.h"

#include <stdio.h>
#include "debug/iscsi_debug.h"

#ifdef WIN32

static void* buff_malloc(size_t size)
{
	DEBUG1("buff_malloc: Allocating %d\n", size);
	return (void*)GlobalAlloc(GMEM_FIXED, size);
}

static void buff_free(void *mem)
{
	DEBUG("buff_free: Freeing \n");
	GlobalFree((HGLOBAL)mem);
}

#else
#define buff_free		free
#define buff_malloc		malloc
#endif


/*
#define MEM_FILL_TEST
*/

struct Buffer *allocLocalBuff(struct BufferAllocator *ba, uint32_t size) 
{
	struct BufferAllocator *b = ba;
	struct BufferAllocator *pref = NULL;
	struct Buffer *nb;

	do {
		if ((b->ap.maxLength >= size) && (b->buffFree != NULL)) {
			if (pref != NULL) {
				if ((pref->ap.maxLength > b->ap.maxLength)) {
					pref = b;
				}
			} else {
				pref = b;
			}
			
		}
		b = (struct BufferAllocator *)b->list.next;
	} while (b != ba);
	
	if (pref != NULL) {
		nb = (struct Buffer *)allocMoveNode((struct DCirList **)&pref->buffFree, 
			(struct DCirList **)&pref->buffUsed);
		if (nb != NULL) {
			pref->frees--;
			pref->useds++;
		}

#ifdef  MEM_FILL_TEST
		memset(nb->data, 'M', nb->maxLength - sizeof(struct _Buffer));
#endif
		return nb;
	}
	return NULL;
}



int AllocBuffers(struct BufferAllocator *ba) 
{
	unsigned i, count;
	struct Buffer *nb;
	
	if ((ba->buffFree == NULL) && (ba->buffUsed == NULL)) {
		count = ba->ap.nIntialCount;
		ba->useds = 0;
		ba->frees = 0;
	} else {
		count = ba->ap.nIncrease;
	}

	for (i = 0; i < count; i++) {
		nb = (struct Buffer *)buff_malloc(ba->ap.maxLength + sizeof(struct Buffer));
		if (nb == NULL) {
			ba->frees += i;
			return (-(int)i);
		}
		nb->list.next = (struct DCirList *)nb;
		nb->list.prev = (struct DCirList *)nb;
		
		/*addSNode((struct DCirList *)nb, (struct DCirList **)&ba->buffFree);*/
		addNode((struct DCirList *)nb, (struct DCirList **)&ba->buffFree);
		
		nb->belong = ba;
		nb->length = 0;
		nb->maxLength = ba->ap.maxLength;
	}

	ba->frees += i;
	return i;
}


int bufferAllocatorInit(struct BufferAllocator *ba, struct AllocParam *prm, uint32_t sizeCounts)
{
	int res;
	unsigned i = sizeCounts - 1;

	ba[i].list.next = (struct DCirList *)&ba[0];
	ba[i].list.prev = (struct DCirList *)&ba[i - 1];
	ba[i].ap = prm[i];
	ba[i].buffUsed = NULL;
	ba[i].buffFree = NULL;
	ba[i].parnetAllocator = NULL;
	AllocBuffers(&ba[i]);
	for (--i; i > 0; i--) {
		ba[i].list.next = (struct DCirList *)&ba[i + 1];
		ba[i].list.prev = (struct DCirList *)&ba[i - 1];
		ba[i].ap = prm[i];
		ba[i].buffUsed = NULL;
		ba[i].buffFree = NULL;
		ba[i].parnetAllocator = NULL;
		AllocBuffers(&ba[i]);
	}
	ba[0].list.next = (struct DCirList *)&ba[1];
	ba[0].list.prev = (struct DCirList *)&ba[sizeCounts - 1];
	ba[0].ap = prm[0];
	ba[0].buffUsed = NULL;
	ba[0].buffFree = NULL;
	ba[0].parnetAllocator = NULL;
	res = AllocBuffers(&ba[0]);

	if (res > 0) {
		return 0;
	} else {
		bufferAllocatorClean(ba, sizeCounts);
		return -1;
	}
}

struct BufferAllocator *bufferAllocatorCreate(struct AllocParam *prm, uint32_t sizeCounts)
{
	struct BufferAllocator *ba;

	ba = (struct BufferAllocator *)malloc(sizeof(struct BufferAllocator) * sizeCounts);
	if (ba == NULL) {
		return NULL;
	}

	bufferAllocatorInit(ba, prm, sizeCounts);

	return ba;
}

int freeMemoryBuff(struct BufferAllocator *ba)
{
	int i;
	struct Buffer *bu;
	
	for (i = 0; i <  (int)ba->ap.nDecrease; i++) {
		bu = (struct Buffer *)removeNode((struct DCirList *)ba->buffFree->list.prev, 
			(struct DCirList **)&ba->buffFree);
		buff_free(bu);
	}

	ba->frees -= i;
	return i;
}


struct Buffer *allocBuff(struct BufferAllocator *ba, uint32_t size) 
{
	/* FIXME! parent usage */
	struct BufferAllocator *pref;
	struct BufferAllocator *b;

	struct Buffer *nb = allocLocalBuff(ba, size);
	if (nb != NULL) {
		return nb;
	}

	b = ba;
	pref = NULL;

	do {
		if ((b->ap.maxLength > size) && (b->ap.nIncrease > 0) && (
			( (pref != NULL) && (pref->ap.maxLength > b->ap.maxLength) ) || (pref == NULL))  ) {
			pref = b;
		}
		b = (struct BufferAllocator *)b->list.next;
	} while (b != ba);

	if (pref != NULL) {
		AllocBuffers(pref);
		return allocLocalBuff(ba, size);
	} else {
		DEBUG("allocBuff: can't find pref zone\n");
		return NULL;
	}

	return NULL;
}

int freeBuff(struct Buffer *buff) 
{
	/* FIXME! parent usage */
	struct Buffer *bu = (struct Buffer *)moveNode((struct DCirList *)buff, 
			(struct DCirList **)&buff->belong->buffUsed, (struct DCirList **)&buff->belong->buffFree);
	if (bu != NULL) {
		buff->belong->frees++;
		buff->belong->useds--;

		if ((buff->belong->frees >= (uint32_t)(buff->belong->ap.nDecrease + buff->belong->ap.nIncrease)) &&
			(buff->belong->frees + buff->belong->useds > buff->belong->ap.nIntialCount) && 
			(buff->belong->ap.nDecrease > 0)) {
			return freeMemoryBuff(buff->belong);
		}
	}
	return (bu == NULL);
}

void bufferAllocatorCleanBuffers(struct BufferAllocator *ba)
{
	struct Buffer *b;

	if (ba->buffUsed != NULL) {
		DEBUG1("bufferAllocatorCleanBuffers: %d blocks are used!\n", ba->useds); 
	}
	
	while (ba->buffUsed != NULL) {
		b = (struct Buffer *)removeNode(ba->buffUsed->list.prev, 
				(struct DCirList **)&ba->buffUsed);
		if (b == NULL) {
			break;
		} 		
		buff_free(b);
		/*ba->useds--;*/
	}
	
	while (ba->buffFree != NULL) {
		b = (struct Buffer *)removeNode(ba->buffFree->list.prev, 
				(struct	DCirList **)&ba->buffFree);
		if (b == NULL) {
			break;
		} 		
		buff_free(b);
		/*ba->frees--;*/
	}

}


void bufferAllocatorClean(struct BufferAllocator *ba, unsigned int count)
{
	for (; count > 0; count--) {
		bufferAllocatorCleanBuffers(&ba[count - 1]);
	}
}





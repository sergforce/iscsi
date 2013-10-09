// buffer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>

#define DEBUG(x) printf(x)
#define DEBUG1(x, y) printf(x, y)

struct DCirList *allocNode(struct DCirList **freeList)
{
	struct DCirList *node;
	struct DCirList *frees = *freeList;
	
	if (frees == NULL) {
		return NULL; /* no free lists */
	}

	node = frees->prev;
	if (node == frees) {
		*freeList = NULL;
	} else {
		frees->prev = node->prev;
		node->prev->next = frees;
	}
	
	node->next = node;
	node->prev = node;
	return node;
}

struct DCirList *allocMoveNode(struct DCirList **frees, struct DCirList **used)
{
	struct DCirList *node;
	struct DCirList *freeList = *frees;
	struct DCirList *usedList = *used;

	if (freeList == NULL) {
		return NULL; /* no free lists */
	}

	node = freeList->prev;
	if (node == freeList) {
		*frees = NULL;
	} else {
		freeList->prev = node->prev;
		node->prev->next = freeList;
	}
	
	if (usedList == NULL) {
		*used = node;
		node->next = node;
		node->prev = node;
	} else {
		usedList->prev->next = node;
		node->next = usedList;

		node->prev = usedList->prev;
		usedList->prev = node;
	}
	return node;
}

struct DCirList *removeNode(struct DCirList *node, struct DCirList **listFrom)
{	
	struct DCirList *from = *listFrom;

	if (from == NULL) {
		DEBUG("!!! listFrom == NULL\n");
		return NULL;
	}

	if (from->next == from) {
		if (node != from ) {
			DEBUG("List contain one element, but not node \n");
			return NULL;
		}
		
		*listFrom = NULL;
	} else {		
		if (from == node) {
			*listFrom = from->next;
		}

		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
	
	node->prev = node;
	node->next = node;
	return node;
}

struct DCirList *addNode(struct DCirList *node, struct DCirList **listTo)
{
	struct DCirList *to = *listTo;
	struct DCirList *tmp;

	if (to == NULL) {
		*listTo = node;
	} else {
		to->prev->next = node;
		node->prev->next = to;

		tmp = node->prev;
		node->prev = to->prev;
		to->prev = tmp;
	}
	return to;
}

struct DCirList *addSNode(struct DCirList *node, struct DCirList **listTo)
{
	struct DCirList *to = *listTo;

	if (to == NULL) {
		node->next = node;
		node->prev = node;
		*listTo = node;
	} else {
		to->prev->next = node;
		node->next = to;

		node->prev = to->prev;
		to->prev = node;
	}
	return to;
}


struct DCirList *moveNode(struct DCirList *node, struct DCirList **listFrom, struct DCirList **listTo)
{
	if (removeNode(node, listFrom) == node) {
		return addNode(node, listTo);
	}
	return NULL;
}


struct Buffer *allocLocalBuff(struct BufferAllocator *ba, uint32_t size) 
{
	struct BufferAllocator *b = ba;
	struct BufferAllocator *pref = ba;
	struct Buffer *nb;

	do {
		if ((b->ap.maxLength > size) && (pref->ap.maxLength > b->ap.maxLength) &&
			(b->buffFree != NULL)) {
			pref = b;
		}
		b = (struct BufferAllocator *)b->list.next;
	} while (b != ba);

	nb = (struct Buffer *)allocMoveNode((struct DCirList **)&pref->buffFree, 
		(struct DCirList **)&pref->buffUsed);
	if (nb != NULL) {
		pref->frees--;
		pref->useds++;
	}
	return nb;
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
		nb = (struct Buffer *)malloc(ba->ap.maxLength + sizeof(struct Buffer));
		if (nb == NULL) {
			ba->frees += i;
			return i;
		}
		addSNode((struct DCirList *)nb, (struct DCirList **)&ba->buffFree);
		nb->belong = ba;
		nb->length = 0;
		nb->maxLength = ba->ap.maxLength;
	}

	ba->frees += i;
	return i;
}

void bufferAllocatorInit(struct BufferAllocator *ba, struct AllocParam *prm, uint32_t sizeCounts)
{
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
	AllocBuffers(&ba[0]);

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
		free(bu);
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
	pref = ba;

	do {
		if ((b->ap.maxLength > size) && (pref->ap.maxLength > b->ap.maxLength)) {
			pref = b;
		}
		b = (struct BufferAllocator *)b->list.next;
	} while (b != ba);

	AllocBuffers(pref);

	return allocLocalBuff(ba, size);
}

int freeBuff(struct Buffer *buff) 
{
	/* FIXME! parent usage */
	struct Buffer *bu = (struct Buffer *)moveNode((struct DCirList *)buff, 
			(struct DCirList **)&buff->belong->buffUsed, (struct DCirList **)&buff->belong->buffFree);
	if (bu != NULL) {
		buff->belong->frees++;
		buff->belong->useds--;

		if ((buff->belong->frees >= (uint32_t)(buff->belong->ap.nDecrease + buff->belong->ap.nIncrease) ) &&
			(buff->belong->frees + buff->belong->useds > buff->belong->ap.nIntialCount)) {
			return freeMemoryBuff(buff->belong);
		}
	}
	return (bu == NULL);
}

void bufferAllocatorClean(struct BufferAllocator *ba)
{
	struct Buffer *buff;
	struct Buffer *b;
	buff = ba->buffUsed;

	if (buff != NULL) {
		DEBUG1("bufferAllocatorClean: %d blocks are used!\n", ba->useds);
	}
	
	while (buff != NULL) {
		b = (struct Buffer *)removeNode((struct DCirList *)buff->list.prev, (struct DCirList **)&buff);
		if (b == NULL) {
			break;
		} 		
		free(b);
		/*ba->useds--;*/
	}
	
	buff = ba->buffFree;
	while (buff != NULL) {
		b = (struct Buffer *)removeNode((struct DCirList *)buff->list.prev, (struct DCirList **)&buff);
		if (b == NULL) {
			break;
		} 		
		free(b);
		/*ba->frees--;*/
	}

}

/***************************************************************************************************/

void createStaticList(void *list, uint32_t structSize, uint32_t sizeCounts)
{
	unsigned i = sizeCounts - 1;

	((struct DCirList *)list)->next = 
		(struct DCirList *)((uint8_t *)list + structSize);
	((struct DCirList *)list)->prev = 
		(struct DCirList *)((uint8_t *)list + structSize * (sizeCounts - 1));

	for (i = 1; i < sizeCounts - 1; i++) {
		((struct DCirList *)((uint8_t *)list + structSize * i))->next = 
			(struct DCirList *)((uint8_t *)list + structSize * (i + 1));
		((struct DCirList *)((uint8_t *)list + structSize * i))->prev = 
			(struct DCirList *)((uint8_t *)list + structSize * (i - 1));

	}

	((struct DCirList *)((uint8_t *)list + structSize * i))->next = 
		(struct DCirList *)list;
	((struct DCirList *)((uint8_t *)list + structSize * i))->prev = 
		(struct DCirList *)((uint8_t *)list + structSize * (i - 1));
}


#define BUFF_COUNT 4


#ifdef COUNTER_64

#define COUNTER_MASK			0x8000000000000000
#define COUNTER_BITS			64
typedef uint64_t counter_t;

#else

#define COUNTER_MASK			0x80000000
#define COUNTER_BITS			32
typedef uint32_t counter_t;

#endif

struct Counter {
	uint32_t count;
	counter_t mask;
};


#define COUNTER_REPAIRED		2
#define COUNTER_IN_ORDER		1
#define COUNTER_OUT_OF_ORDER	0
#define COUNTER_ERROR			-1

void counterInit(struct Counter *cnt, uint32_t value)
{
	cnt->count = value;
	cnt->mask = 0;
}

int counterAddValue(struct Counter *cnt, uint32_t value)
{
	int k = value - cnt->count;
	counter_t msk;

	if ((cnt->mask == 0) && (k == 1)) {
		cnt->count++;
		return COUNTER_IN_ORDER;
	}

	if (k > COUNTER_BITS) {
		DEBUG("counterAddValue: more losses!\n");
		return COUNTER_ERROR;
	}

	if (k != 1) {
		cnt->mask |= COUNTER_MASK >> (k - 1);
		return COUNTER_OUT_OF_ORDER;
	} else {
		for (msk = (cnt->mask << 1), k = 1; (msk & COUNTER_MASK) != 0; k++) {
			msk <<= 1;
		}
		cnt->mask = msk;
		cnt->count += k;
		return COUNTER_REPAIRED;
	}
}

int main(int argc, char* argv[])
{
	Buffer buff[BUFF_COUNT];
	int i;
	BufferAllocator back;
	BufferAllocator back2;
	BufferAllocator *back3;
	Buffer *al1, *al2, *al3, *al4, *al5 ;
	Buffer *nm;
	AllocParam prm[3] = {		
		{65536, 2, 4, 2},
		{16384, 2, 4, 2},
		{4096, 4, 8, 16}
	};

	struct Counter cnt;

	counterInit(&cnt, 0);
	counterAddValue(&cnt, 1);
	counterAddValue(&cnt, 2);
	counterAddValue(&cnt, 4);
	counterAddValue(&cnt, 5);
	counterAddValue(&cnt, 6);
	counterAddValue(&cnt, 7);
	counterAddValue(&cnt, 10);
	counterAddValue(&cnt, 3);
	counterAddValue(&cnt, 9);
	counterAddValue(&cnt, 8);

	createStaticList(buff, sizeof(Buffer), BUFF_COUNT);

	/*
	buff[0].list.next = (struct DCirList *)&buff[1];
	buff[0].list.prev = (struct DCirList *)&buff[BUFF_COUNT - 1];
	for (i = 1; i < BUFF_COUNT - 1; i++) {
		buff[i].list.next = (struct DCirList *)&buff[i + 1];
		buff[i].list.prev = (struct DCirList *)&buff[i - 1];
	}
	buff[i].list.next = (struct DCirList *)&buff[0];
	buff[i].list.prev = (struct DCirList *)&buff[i - 1];
*/

	back.buffFree = buff;
	back.buffUsed = NULL;

	al1 = (struct Buffer *)allocMoveNode((struct DCirList **)&back.buffFree, (struct DCirList **)&back.buffUsed);
	al2 = (struct Buffer *)allocMoveNode((struct DCirList **)&back.buffFree, (struct DCirList **)&back.buffUsed);
	al3 = (struct Buffer *)allocMoveNode((struct DCirList **)&back.buffFree, (struct DCirList **)&back.buffUsed);
	al4 = (struct Buffer *)allocMoveNode((struct DCirList **)&back.buffFree, (struct DCirList **)&back.buffUsed);
	al5 = (struct Buffer *)allocMoveNode((struct DCirList **)&back.buffFree, (struct DCirList **)&back.buffUsed);

	back2.buffFree = NULL;
	back2.buffUsed = NULL;
	nm = (struct Buffer *)malloc(0xFFFF);
	addSNode((struct DCirList *)nm, (struct DCirList **)&back2.buffFree);
	nm = (struct Buffer *)malloc(0xFFFF);
	addSNode((struct DCirList *)nm, (struct DCirList **)&back2.buffFree);
	nm = (struct Buffer *)malloc(0xFFFF);
	addSNode((struct DCirList *)nm, (struct DCirList **)&back2.buffFree);
	nm = (struct Buffer *)malloc(0xFFFF);
	addSNode((struct DCirList *)nm, (struct DCirList **)&back2.buffFree);


	back3 = bufferAllocatorCreate(prm, 3);
	

	al1 = allocBuff(back3, 17000);
	al2 = allocBuff(back3, 17000);
	al3 = allocBuff(back3, 17000);
	al4 = allocBuff(back3, 17000);
	al5 = allocBuff(back3, 17000);

	bufferAllocatorClean(back3);

/*	i = freeBuff(al5);
	i = freeBuff(al4);
	i = freeBuff(al3);
	i = freeBuff(al2);
	i = freeBuff(al1);
*/
	

	return 0;
}

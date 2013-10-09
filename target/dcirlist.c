#include "sock.h"
#include "debug/iscsi_debug.h"
#include "dcirlist.h"

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

void addToNodeBefore(struct DCirList *node, struct DCirList *to)
{
	struct DCirList *tmp = node->prev;
	to->prev->next = node;
	node->prev->next = to;

	node->prev = to->prev;
	to->prev = tmp;
}
/* FIXME Not checked!*/
void addToNodeAfter(struct DCirList *node, struct DCirList *to)
{
	struct DCirList *tmp = node->next;
	to->next->prev = node;
	node->next->prev = to;

	node->next = to->next;
	to->next = tmp;
}

struct DCirList *addNode(struct DCirList *node, struct DCirList **listTo)
{
	struct DCirList *to = *listTo;
	struct DCirList *tmp;

	if (to == NULL) {
		to = *listTo = node;
	} else {
		to->prev->next = node;
		node->prev->next = to;

		tmp = node->prev;
		node->prev = to->prev;
		to->prev = tmp;
	}
	return to;
}

/* FIME Maybe serious errors!
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
*/

struct DCirList *moveNode(struct DCirList *node, struct DCirList **listFrom, struct DCirList **listTo)
{
	if (removeNode(node, listFrom) == node) {
		return addNode(node, listTo);
	}
	return NULL;
}



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

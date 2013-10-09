#ifndef __DCIRLIST_H_
#define __DCIRLIST_H_

struct DCirList {
	struct DCirList *next; 
	struct DCirList *prev; 
};

struct DCirList *allocNode(struct DCirList **freeList);
struct DCirList *allocMoveNode(struct DCirList **frees, struct DCirList **used);
struct DCirList *removeNode(struct DCirList *node, struct DCirList **listFrom);

void addToNodeBefore(struct DCirList *node, struct DCirList *to);
struct DCirList *addNode(struct DCirList *node, struct DCirList **listTo);
/*struct DCirList *addSNode(struct DCirList *node, struct DCirList **listTo);*/
struct DCirList *moveNode(struct DCirList *node, struct DCirList **listFrom, struct DCirList **listTo);

void createStaticList(void *list, uint32_t structSize, uint32_t sizeCounts);

#endif

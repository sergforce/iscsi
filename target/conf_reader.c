#include "sock.h"
#include "dcirlist.h"
#include "iscsi_mem.h"
#include "debug/iscsi_debug.h"
#include "conf_reader.h"
#include <stdio.h>

void cleanConfiguration(struct configuration *cnf)
{
	createStaticList(cnf->pool, sizeof(struct confElement), POOL_SIZE);
	cnf->free = cnf->pool;
	cnf->head = NULL;
}

struct confElement *findElement(struct confElement *el, const char *name)
{
	int res;
	struct confElement *elFirst = el;
	if (el == NULL) {
		return NULL;
	}

	do {
		res = strcmp(el->name, name);
		if (res == 0) {
			return el;
		}
		el = el->list.next;
	} while (el != elFirst);

	return NULL;
}

struct confElement *findMain(struct configuration *cnf, const char *name)
{
	return findElement(cnf->head, name);
}


const char *getValue(struct confElement *main, const char *name)
{
	struct confElement *el = findElement(main, name);
	return ((el != NULL) ? el->value : NULL);
}

const char *getValueDef(struct confElement *main, const char *name, const char *def)
{
	const char *val = getValue(main, name);
	return ((val != NULL) ? val : def);
}

#define MIN(x,y) (((x) > (y)) ? (y) : (x))

struct confElement *allocElement(struct configuration *cnf)
{
	return (struct confElement *)allocNode((struct DCirList **)&cnf->free);
}

struct confElement *addElementFill(struct configuration *cnf, struct confElement *el, const char *name, const char *value)
{
	struct confElement *elNew = allocElement(cnf);	
	if (elNew == NULL) {
		return NULL;
	}

	elNew->parent = el;
	elNew->childs = NULL;

	elNew->cbName = MIN(strlen(name), NAME_LEN);
	elNew->cbValue = MIN(strlen(value), VALUE_LEN);

	strncpy(elNew->name, name, NAME_LEN);
	strncpy(elNew->value, value, VALUE_LEN);

	if (el) {
		addNode((struct DCirList *)elNew, (struct DCirList **)&el->childs);
	} else if (cnf->head) {
		addNode((struct DCirList *)elNew, (struct DCirList **)&cnf->head);
	} else {
		cnf->head = elNew;
	}
	return elNew;
}

struct confElement *addElement(struct confElement *elNew, struct confElement **elAfter)
{	
	return (struct confElement *)addNode((struct DCirList *)elNew, (struct DCirList **)elAfter);
}

struct confElement *addMain(struct configuration *cnf, const char *name, const char *value)
{
	return addElementFill(cnf, cnf->head, name, value);
}



void removeElement(struct configuration *cnf, struct confElement *el)
{
	while (el->childs != NULL) {
		removeElement(cnf, el->childs);
	}

	if (el->parent == NULL) {
		moveNode((struct DCirList *)el, (struct DCirList **)&cnf->head, (struct DCirList **)&cnf->free);
	} else {
		moveNode((struct DCirList *)el, (struct DCirList **)&el->parent->childs, 
											(struct DCirList **)&cnf->free);
	}
}


struct confElement *getElemInt(struct confElement *head, const char *paramName, int *value)
{
	struct confElement *el = findElement(head->childs, paramName);
	if (el) {
		if (sscanf(el->value, "%d", value) == 1) {
			return el;
		}
	}
	return NULL;
}

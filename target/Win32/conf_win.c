#include "../sock.h"
#include "../dcirlist.h"
#include "../iscsi_mem.h"
#include "../debug/iscsi_debug.h"
#include "../conf_reader.h"

#include <tchar.h>
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static int windowsRegInit(struct configuration *cnf, struct confElement *elParent, HKEY phkResParent);

/*
*/

int initConfiguration(struct configuration *cnf)
{
	cleanConfiguration(cnf);
	return windowsRegInit(cnf, NULL, NULL);
}


static int windowsRegInit(struct configuration *cnf, struct confElement *elParent, HKEY phkResParent)
{
	LONG res;
	HKEY phkRes;
	DWORD i, type;
	int count = 0;
	struct confElement *el, *elNew = NULL;

	if (elParent == NULL) {
		res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, MAIN_TREE, 0, KEY_QUERY_VALUE, &phkRes);
	} else {
		res = RegOpenKeyEx(phkResParent, elParent->name, 0, KEY_QUERY_VALUE, &phkRes);
	}

	if (res != ERROR_SUCCESS) {
		return 0;
	}
	
	el = (struct confElement *)allocNode((struct DCirList **)&cnf->free);
	if (el == NULL) {
		DEBUG("windowsRegInit: need more buffer!\n");
		RegCloseKey(phkRes);
		return count;
	}
	el->parent = elParent;
	el->childs = NULL;

	if (elParent == NULL) {
		cnf->head = el;
		
	} else {
		elParent->childs = el;
	}

	
	for (i = 0; res == ERROR_SUCCESS; i++) {
		el->cbName = NAME_LEN;
		el->cbValue = VALUE_LEN;
		type = REG_SZ;
		res = RegEnumValue(phkRes, i, el->name, &el->cbName, NULL, &type, el->value, &el->cbValue);

		if ((res == ERROR_SUCCESS) && (type == REG_SZ)) {
			if (el->name[0] != 0) {
				windowsRegInit(cnf, el, phkRes);
			}
			count++;

			elNew = (struct confElement *)allocNode((struct DCirList **)&cnf->free);
			if (elNew == NULL) {
				DEBUG("windowsRegInit: need more buffer!\n");
				RegCloseKey(phkRes);
				return count;
			}
			addNode((struct DCirList *)elNew, (struct DCirList **)&el);
			el = elNew;
			el->parent = elParent;
			el->childs = NULL;

		}
	}

	if (elParent == NULL) {
		removeNode((struct DCirList *)el, (struct DCirList **)&cnf->head);
	} else {
		removeNode((struct DCirList *)el, (struct DCirList **)&elParent->childs);
	}

	addNode((struct DCirList *)el, (struct DCirList **)&cnf->free);
	RegCloseKey(phkRes);
	return count;
}


static int windowsRegSync(struct configuration *cnf, struct confElement *elParent, HKEY phkResParent)
{
	LONG res;
	HKEY phkRes;
	int count = 0;
	struct confElement *el, *elFirst;

	if (elParent == NULL) {
		res = SHDeleteKey(HKEY_LOCAL_MACHINE, MAIN_TREE);
		res = RegCreateKeyEx(HKEY_LOCAL_MACHINE, MAIN_TREE, 0, NULL, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, NULL, &phkRes, NULL);
	} else {
		res = RegCreateKeyEx(phkResParent, elParent->name, 0, NULL, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, NULL, &phkRes, NULL);
	}

	if (res != ERROR_SUCCESS) {
		return -1;
	}

	if (elParent == NULL) {
		el = elFirst = cnf->head;
	} else {
		el = elFirst = elParent->childs;
	}

	do {
		res = RegSetValueEx(phkRes, el->name, 0, REG_SZ, el->value, el->cbValue);
		if (res != ERROR_SUCCESS) {
			return -1;
		}

		if (el->childs != NULL) {
			windowsRegSync(cnf, el, phkRes);
		}


		count++;
		el = el->list.next;
	} while (elFirst != el);

	return count;
}


int syncConfiguration(struct configuration *cnf)
{
	return windowsRegSync(cnf, NULL, NULL);
}


/***************************************************************************************/
#if 0
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

	addNode((struct DCirList *)elNew, (struct DCirList **)&el->childs);
	return elNew;
}

struct confElement *addElement(struct confElement *elNew, struct confElement *el)
{	
	return (struct confElement *)addNode((struct DCirList *)elNew, (struct DCirList **)&el);
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

#endif
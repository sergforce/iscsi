#include "../sock.h"
#include "../dcirlist.h"
#include "../iscsi_mem.h"
#include "../debug/iscsi_debug.h"
#include "../conf_reader.h"

#include <tchar.h>
#include <windows.h>
#include <shlwapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "shlwapi.lib")
#endif

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
		res = RegEnumValue(phkRes, i, el->name, (LPDWORD)&el->cbName, NULL, &type, el->value, (LPDWORD)&el->cbValue);

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
			/*addNode((struct DCirList *)elNew, (struct DCirList **)&el);*/
			addToNodeBefore((struct DCirList *)elNew, (struct DCirList *)el);
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





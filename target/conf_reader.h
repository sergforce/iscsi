#ifndef _CONF_READER
#define _CONF_READER

#define NAME_LEN     110
#define VALUE_LEN    120

#define POOL_SIZE    128

struct confElement;

struct confElemList {
	struct confElement *next;
	struct confElement *prev;
};

struct confElement {
	/* struct DCirList list; */
	struct confElemList list;
	struct confElement *parent;	
	struct confElement *childs;
	int cbName;
	int cbValue;
	char name[NAME_LEN];
	char value[VALUE_LEN];
};

struct configuration {
	struct confElement *head;
	struct confElement *free;
	struct confElement pool[POOL_SIZE];
};


struct confElement *findMain(struct configuration *cnf, const char *name);

struct confElement *findElement(struct confElement *el, const char *name);

const char *getValue(struct confElement *main, const char *name);
const char *getValueDef(struct confElement *main, const char *name, const char *def);

int initConfiguration(struct configuration *cnf);
void cleanConfiguration(struct configuration *cnf);

struct confElement *allocElement(struct configuration *cnf);

struct confElement *addMain(struct configuration *cnf, const char *name, const char *value);
struct confElement *addElementFill(struct configuration *cnf, struct confElement *el, const char *name, const char *value);
struct confElement *addElement(struct confElement *elNew, struct confElement **elAfter);

int syncConfiguration(struct configuration *cnf);

void removeElement(struct configuration *cnf, struct confElement *el);


struct confElement *getElemInt(struct confElement *head, const char *paramName, int *value);



#ifdef WIN32
#define MAIN_TREE	_T("SOFTWARE\\iSCSI Target")
#endif

#ifdef WIN32
#define TARGETS		TEXT("Targets")
#else
#define TARGETS		"Targets"
#endif

#endif

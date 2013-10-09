#ifndef _SPEC_H_
#define _SPEC_H_

#ifdef WIN32
#include "Win32/spec_win32.h"
#else
#include "Linux/spec_linux.h"
#endif


#ifndef _INTERNAL_SPEC
extern struct tfClass knownClasses[];
#endif


struct World;
int createNewClientThread(SOCKET scli);
void initWorldMutex(struct World *world);
int lockWorld(struct World *wrld);
int releaseWorld(struct World *wrld);
void freeWorldMutex(struct World *world);

int beginigAcceptConnections(struct World *world);


#endif

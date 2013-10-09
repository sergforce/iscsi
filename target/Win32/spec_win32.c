#include "../sock.h"
#include "../iscsi.h"
#include "../dcirlist.h"
#include "../iscsi_mem.h"
#include "../tf.h"
#include "../iscsi_param.h"
#include "../conf_reader.h"

#define _INTERNAL_SPEC
#include "../iscsi_session.h"
#include "../contrib/crc32.h"

#include <stdio.h>
#include <stdlib.h>

#include "../debug/iscsi_debug.h"

#include "tf_miniport.h"
#include "wnaspi/srbcmn.h"
#include "wnaspi/srb32.h"
#include "tf_aspi.h"
#include "service.h"

#include "../target_conf.h"


/* in target.c */
int doClient2(SOCKET scli);
/*             */

struct tfClass knownClasses[] = {
	MINIPORT_CLASS_INITIALIZATOR,
	ASPI_CLASS_INITIALIZATOR,
	END_OF_CLASS_INITIALIZATOR
};

DWORD WINAPI doNewClient(LPVOID data)
{
	SOCKET scli = (SOCKET)data;

	return (DWORD)doClient2(scli);
}

int createNewClientThread(SOCKET scli)
{
	HANDLE hThread = CreateThread(NULL, 0, doNewClient, (LPVOID)scli, 0, NULL);
	return (hThread == 0) ? -1 : 0;
}


void initWorldMutex(struct World *world)
{
	world->platformData.hMutex = CreateMutex(NULL, FALSE, FALSE);
}

int lockWorld(struct World *wrld)
{
	WaitForSingleObject(wrld->platformData.hMutex, INFINITE);
	return 0;
}

int releaseWorld(struct World *wrld)
{
	ReleaseMutex(wrld->platformData.hMutex);
	return 0;
}

void freeWorldMutex(struct World *world)
{
	CloseHandle(world->platformData.hMutex);
}


int beginigAcceptConnections(struct World *world)
{
#ifdef _SERVICE
	SetEvent(hRunningService);
#endif
	return 0;
}


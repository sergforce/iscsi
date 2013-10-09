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
#include "../target_conf.h"
#include "tf_sg.h"

/* in target.c */
int doClient2(SOCKET scli);
/*             */

struct tfClass knownClasses[] = {
	SG_CLASS_INITIALIZATOR,
	END_OF_CLASS_INITIALIZATOR
};

void *threadRoutine(void *arg)
{
	int sockfd = (int)arg;
	pthread_detach(pthread_self());
	return (void *)doClient2(sockfd);	
}

int createNewClientThread(SOCKET scli)
{
	pthread_t tid;
	return pthread_create(&tid, NULL, threadRoutine, (void *)scli);
}
void initWorldMutex(struct World *world)
{
	pthread_mutex_init(&world->platformData.mutex, NULL);
}

int lockWorld(struct World *world)
{
	pthread_mutex_lock(&world->platformData.mutex);
	return 0;
}

int releaseWorld(struct World *world)
{
	pthread_mutex_unlock(&world->platformData.mutex);
	return 0;
}

void freeWorldMutex(struct World *world)
{
}

int beginigAcceptConnections(struct World *world)
{
	return 0;
}

#define DEFAULT_TARGET_NAME "iqn:deftarget"
#define DEFAULT_TARGET_DEV  "/dev/sg1"
/* creating BOGUS configuration */
int initConfiguration(struct configuration *cnf)
{
	struct confElement *targets, *defTarget, *defTargetDevName;
	
	cleanConfiguration(cnf);
	targets = addMain(cnf, TARGETS, "");
	defTarget = addElementFill(cnf, targets, DEFAULT_TARGET_NAME, SG_DRIVER_NAME);
	defTargetDevName = addElementFill(cnf, defTarget, SG_DEVICE_NAME, DEFAULT_TARGET_DEV);
		
	return 0;
}

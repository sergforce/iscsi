#ifndef _iSCSI_SESSION_
#define _iSCSI_SESSION_

#include "spec.h"

#define MAX_CDB_LENTH			16
#define TASK_LIST				64
#define PDU_LIST				128

#define STD_RES_BUFFER			16

#define MAX_BUFF_CAT			16


#define RWPDU_SES_ERROR	-3
#define RWPDU_SHUTDOWN  -2
#define RWPDU_ERROR		-1
#define RWPDU_NOTALL	0
#define RWPDU_OK		1
#define RWPDU_NOTHING	2

#define RWOP_TRANS_OK		1
#define RWOP_TRANS_NOT_ALL	0
#define RWOP_ERROR			-1
#define RWOP_SHUTDOWN		-2


/***********************************************************************/
/*                        Counter operations                           */
/***********************************************************************/


#ifdef COUNTER_64

#define COUNTER_MASK			0x8000000000000000
#define COUNTER_BITS			64
typedef uint64_t counter_t;

#else

#define COUNTER_MASK			0x80000000
#define COUNTER_BITS			32
typedef uint32_t counter_t;

#endif

#define SERIAL_BITS				32

#define COUNTER_DUP				3
#define COUNTER_REPAIRED		2
#define COUNTER_IN_ORDER		1
#define COUNTER_OUT_OF_ORDER	0
#define COUNTER_ERROR			-1

struct Counter {
	uint32_t count;
	counter_t mask;
};

void counterInit(struct Counter *cnt, uint32_t value);
int counterAddValue(struct Counter *cnt, uint32_t value);
uint32_t counterGetMaxValue(struct Counter *cnt);

int sIsSmaller(uint32_t s1, uint32_t s2);
int sIsLarger(uint32_t s1, uint32_t s2);


/***********************************************************************/
/*                       ReadWrite operations                          */
/***********************************************************************/

#define RWOP_CRCFLAG	0x80000
#define RWOP_STAGEMASK	0x0ffff

struct ReadWriteOP {
	int stage;	/* CRC Flag 0x8000 */

	uint32_t expectedLen;
	uint32_t perfomedLen;

	uint8_t *buffer;


	uint32_t dataCRC;

};

struct PDUList {
	struct DCirList list;
	struct PDU pdu;
};

struct R2T {
	struct DCirList list;
	uint32_t r2tSN;
	struct Buffer *wBuffer;
/*	struct ResponseTask *tsk; */
};

#define MAX_UNSOLICATEDR2T		1

struct Task {
	struct DCirList list;
	uint32_t cmdSN;
	uint32_t initiatorTaskTag;
	uint64_t lun;

	uint32_t nPDU;
	struct PDUList *pdus;

	struct Buffer *dataSegment;

	struct Counter unsolDataSN; /* for unsolicated SCSI Data-Out */
	uint32_t dataSNr2tSN; /* for SCSI Data-In and R2T */

	/* struct  unsolicatedR2T[MAX_UNSOLICATEDR2T]; */

	struct R2T *r2tRes;
	struct ResponseTask *intialRes;

	uint32_t immediate;
	uint32_t nR2t;

	struct tfCommand cmd;
};

#define RT_FLAG_USER_MASK	0x0ff
#define RT_FLAG_ACK			0x200
#define RT_FLAG_TASK_FREE	0x100

#define RT_FLAG_CAN_FREE	0x300 /* Acknowledged and task free */

struct ResponseTask {
	struct DCirList list;	

	uint32_t statSN;
	
	uint32_t targetTransferTag;
	uint32_t refITT;
	uint64_t lun;

	unsigned iFlags; 

	struct PDUList *pdus;
		
	struct R2T r2t;
	struct Counter dataSN; /* for SCSI Data-Out */

	struct Task *origTask;
	struct ResponseTask *ackTasks;

	struct Buffer *dataSegment;

};

#define MAX_AUTH_PDU	10

struct AuthProc {
	int authType;
	int availAuthTypes;
	
	uint32_t CSG;
	uint32_t NSG;


	int transit;

	int firstPDU;
	uint32_t cmdSN;
	uint32_t statSN;

	uint32_t CID;

	uint32_t initiatorTaskTag;

	struct PDUList *pending;
	struct PDUList *wpending;

	unsigned nFreePDUs;
	struct PDUList *freePDUs;
	struct PDUList freePDU[MAX_AUTH_PDU];

};

struct InetConnection {
	struct DCirList list;

	/* session own, valid if currentStage == FFP */
	struct Session *member;
	struct World *world; /*for special reason, auth, etc.*/
	uint32_t connectionID; /* CID */
	uint32_t currentITT;
	
	SOCKET scli; /* client inet socket */

	int currentStage;	

	struct Counter recvStatSN; /* sended */
	uint32_t statSN;

	struct ResponseTask *sending; /* sendeding  */
	
	struct PDUList *sendedPDUs;

//	struct PDUList *freePDUs;

	struct PDUList *pending;
	struct PDUList *wpending;
	struct Task *pndTask; 

	uint32_t targetMaxRcvDataSegment; /* Default is 8192 bytes */

	/* connection parameters only */
	int headerDigest; /* 0 - None, 1 - CRC32C, 0x10 Ack CRC32 */
	int dataDigest;   /* 0 - None, 1 - CRC32C, 0x10 Ack CRC32 */
	uint32_t initiatorMaxRcvDataSegment; /* Default is 8192 bytes */

	uint32_t headerSize; /* sizeof(BHS) or sizeof(BHS) + 4 */

	struct ReadWriteOP readOP;
	struct ReadWriteOP writeOP;

	int sockNeedToWrite; /* set to 1 when needed to write in this connection */
	
/*	struct confElement *targetInfo; */
	
	struct Param *params;
//	struct PDUList pdus[PDU_LIST];

};

struct Session {
	struct DCirList list;

	struct ISID_TSIH sessionID;
	struct InetConnection *connections;
	int nConnections;

	unsigned errorRecoveryLevel;
	unsigned intialR2T;
	unsigned immediateData;


	void *tfHandle;

	/* session parameters only */
	struct Param *params;

	struct Counter cmdSN; /* recived */

	struct Task *notAllRecived;  /* recived task which fragmented */

	struct Task *tasks;          /* pending tasks */
	struct Task *immediateTasks; /* pending immediate tasks */

	struct Task *freeTasks;      /* free tasks */

	struct Task *perfTasks;
	struct Task *perfImTasks;

	struct PDUList *freePDUs;
	
	struct ResponseTask *freeResTasks;
	struct ResponseTask *resTasks; /* need to send */
	struct ResponseTask *sended; /* sended, but not acknowledge */

	uint32_t minMaxRcvDataSegment;

	unsigned nFreeTasks;
	unsigned nFreeResTasks;
	unsigned nFreePDUs;

	struct BufferAllocator buffAlloc[MAX_BUFF_CAT]; /* buffers policy */

	struct Task	taskPool[TASK_LIST];	
	
	struct ResponseTask resp[TASK_LIST];

	struct PDUList pdus[PDU_LIST];

	uint32_t targetTaskTag;
	uint32_t unAckSize;

	struct tfClass cclass;

	unsigned pendingCommands;
	
	SOCKET maxfd;
	/* reading operations */
	fd_set	rop;
	/* writing operations */
	fd_set	wop;

	int connChanged; /* set to 1 when anything happend with connections (Disconnected/Connected) */

	/* benchmarks */
	int64_t bytes_read;
	int64_t bytes_write;
	int64_t pdus_sent;
	int64_t pdus_recv;
};

#define WORLD_BUFFALLOC_COUNT		2
struct World {
	struct Session *sesions;
	unsigned nSessions;
	unsigned nTotalConnections;

	spec_world_t platformData;

	struct confElement *trgs;

	struct configuration cnf;
	struct BufferAllocator buffAlloc[WORLD_BUFFALLOC_COUNT]; /* buffers policy */

	int nApis;
	struct tfClass apis[MAX_TFCLASES];
};


#define WAIT_FD_WRITE 1
#define WAIT_FD_READ  0

int cleanWaitigFd(struct Session *ses, SOCKET fd, int typeRW);
int setWaitigFd(struct Session *ses, SOCKET fd, int typeRW);
int isSetWaitigFd(struct Session *ses, SOCKET fd, int typeRW);
int initWaitigFd(struct Session *ses);

int initWorld(struct World *wrld);


#define WORLD_SMALL_BUFFER		(256 - sizeof(struct Buffer))
#define WORLD_BIG_BUFFER		(65536 - sizeof(struct Buffer))

#define INIT_WORLD_BUFFALLOC		{ \
		{WORLD_BIG_BUFFER, 4, 4, 4}, \
		{WORLD_SMALL_BUFFER, 2, 4, 2} \
	}

#define DEF_MAX_RCV_SEG_LENGTH	8192


#define PDU_OK				 1
#define PDU_CONNSHUTDOWN	 0	
#define PDU_CONNRESET		-1
#define PDU_HEADER_CRC_DM	-2
#define PDU_DATA_CRC_DM		-3
#define PDU_RCVBUF_SMALL    -4
#define PDU_TRCVBUF_SMALL   -5

#define PDU_UNKNOWN_ERROR	-100

int reciveFirstBHS(SOCKET s, struct PDU *pdu, struct timeval *tv);
int reciveFirstData(SOCKET s, struct PDU *pdu, struct timeval *tv);
int reciveFirstPDU(SOCKET s, struct PDU *pdu, struct timeval *tv);

int sendFirstPDU(SOCKET s, struct PDU *pdu, struct timeval *tv);


int sendPDU(struct InetConnection *conn);
int recivePDU(struct InetConnection *conn);


#define SYNC_PORT 30000

#endif

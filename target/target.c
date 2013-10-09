#include "sock.h"
#include "iscsi.h"
#include "dcirlist.h"
#include "iscsi_mem.h"
#include "tf.h"
#include "iscsi_param.h"
#include "conf_reader.h"
#include "iscsi_session.h"
#include "contrib/crc32.h"

#include <stdio.h>
#include <stdlib.h>

#include "debug/iscsi_debug.h"
/*
#ifdef WIN32
#include "Win32/tf_miniport.h"
#include "Win32/tf_aspi.h"
#include "Win32/service.h"
#endif
*/

/*#include "spec.h"*/
#include "target_conf.h"

#define ISCSI_PORT 3260

int doClient(SOCKET scli);
int doClient2(SOCKET scli);

/*
#define DUMP_LOGIN_PDUS
*/

/* extern struct tfClass *knownClasses; */

struct World wrld;

static struct InetConnection *allocInetConnection(struct World *wrld, SOCKET scli);

/*
static struct tfClass knownClasses[] = {
#ifdef WIN32
	MINIPORT_CLASS_INITIALIZATOR,
	ASPI_CLASS_INITIALIZATOR,
#endif	
	END_OF_CLASS_INITIALIZATOR
};
*/

int targetTest(void)
{
	struct sockaddr_in addr_cli;
	int addr_cli_len = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	int res;
	int maxBuff = 65536*2;
	int nagle = 0;

	SOCKET s;
	SOCKET scli;
	

	initWorld(&wrld);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { return -1; }

	addr.sin_family = AF_INET;
	addr.sin_port = htons(ISCSI_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	res = bind(s, (struct sockaddr*)&addr, sizeof(addr));
	if (res < 0) { 
		DEBUG("Can't bind\n");
		return -2; 
	}

	res = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&maxBuff, sizeof(int));
	res = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&maxBuff, sizeof(int));


	res = listen(s, 1);
	if (res < 0) { 		
		DEBUG("Can't listen\n");
		return -3; 
	}
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&nagle, sizeof(int));	

	beginigAcceptConnections(&wrld);
	
	while ((scli = accept(s, (struct sockaddr*)&addr_cli, &addr_cli_len)) != -1) {
		/*Printing accepted from*/		

		DEBUG1("Accepted connection from %x\n", ntohl(addr_cli.sin_addr.s_addr));
		
		setsockopt(scli, IPPROTO_TCP, TCP_NODELAY, (char*)&nagle, sizeof(int));

		createNewClientThread(scli);
		/* CreateThread(NULL, 0, doNewClient, (LPVOID)scli, 0, NULL); */

		/* Creting thread 
		doClient2(scli);*/
	}
	

	return 0;
}

int doConnection(struct InetConnection *conn);

static void initDefParams(struct Param *params)
{
	/* Setting defaults parameters for this session */
	
	params[ISP_MAXCONNECTIONS].ivalue = 1;
	params[ISP_MAXCONNECTIONS].avalue = 0;

	params[ISP_SENDTARGETS].tvalue = NULL;
	params[ISP_SENDTARGETS].avalue = 0;

	params[ISP_TARGETNAME].tvalue = NULL;
	params[ISP_TARGETNAME].avalue = 0;

	params[ISP_INITIATORNAME].tvalue = NULL;
	params[ISP_INITIATORNAME].avalue = 0;

	params[ISP_TARGETALIAS].tvalue = NULL;
	params[ISP_TARGETALIAS].avalue = 0;

	params[ISP_INITIATORALIAS].tvalue = NULL;
	params[ISP_INITIATORALIAS].avalue = 0;

	params[ISP_TARGETADDRESS].tvalue = NULL;
	params[ISP_TARGETADDRESS].avalue = 0;

	params[ISP_TARGETPORTALGROUPTAG].ivalue = 0;
	params[ISP_TARGETPORTALGROUPTAG].avalue = 0;

	params[ISP_TARGETADDRESS].tvalue = NULL;
	params[ISP_TARGETADDRESS].avalue = 0;

	params[ISP_INITIALR2T].tvalue = ISPN_YES;
	params[ISP_INITIALR2T].avalue = 1;

	params[ISP_IMMEDIATEDATA].tvalue = ISPN_YES;
	params[ISP_IMMEDIATEDATA].avalue = 1;

	params[ISP_MAXBURSTLENGTH].ivalue = 262144;
	params[ISP_MAXBURSTLENGTH].avalue = 0;

	params[ISP_FIRSTBURSTLENGTH].ivalue = 65536;
	params[ISP_FIRSTBURSTLENGTH].avalue = 0;

	params[ISP_DEFAULTTIME2WAIT].ivalue = 2;
	params[ISP_DEFAULTTIME2WAIT].avalue = 0;

	params[ISP_DEFAULTTIME2RETAIN].ivalue = 20;
	params[ISP_DEFAULTTIME2RETAIN].avalue = 0;

	params[ISP_MAXOUTSTANDINGR2T].ivalue = 1;
	params[ISP_MAXOUTSTANDINGR2T].avalue = 0;

	params[ISP_DATAPDUINORDER].tvalue = ISPN_YES;
	params[ISP_DATAPDUINORDER].avalue = 1;

	params[ISP_DATASEQUENCEINORDER].tvalue = ISPN_YES;
	params[ISP_DATASEQUENCEINORDER].avalue = 1;

	params[ISP_ERRORRECOVERYLEVEL].ivalue = 0;
	params[ISP_ERRORRECOVERYLEVEL].avalue = 0;

	params[ISP_SESSIONTYPE].tvalue = ISPN_ST_NORMAL;
	params[ISP_SESSIONTYPE].avalue = 0;
}

/*
static struct Param *allocParams(struct World *wrld)
{
	struct Buffer *buff;

	buff = allocBuff(wrld->buffAlloc, sizeof(struct Param) * TOTAL_DEF_PARAMS);
	if (buff == NULL) {
		DEBUG("allocParams: can't allocate params struct!\n");
		return NULL;
	}

	initDefParams((struct Param *)buff->data);
	return (struct Param *)buff->data;
}
*/

/**********************************************************************************/
/**********************************************************************************/


int initWorld(struct World *world)
{
	struct tfClass *cls = knownClasses;
	struct tfClass *wCls = world->apis;
	int i;
	struct AllocParam prm[2] = {		
		{WORLD_BIG_BUFFER, 2, 2, 2},
		{WORLD_SMALL_BUFFER, 2, 4, 2}
	};

	DEBUG("initWorld: Initializing world\n");
	world->sesions = NULL;

	world->nSessions = 0;
	world->nTotalConnections = 0;

	initWorldMutex(world);

	bufferAllocatorInit(world->buffAlloc, prm, 2);

	/* Init configuration */
	i = initConfiguration(&world->cnf);
	if (i < 0) {
		return i;
	}

	world->trgs = findMain(&world->cnf, TARGETS);
	if ((world->trgs == NULL) && (world->trgs->childs == NULL)) {
		DEBUG("No situable configuration found\n");
		return -1;
	}
	world->trgs = world->trgs->childs;

	for (i = 0; cls->className != NULL; cls++) {
		cls->handle = cls->tfInit();
		if (cls->handle) { /* Successfuly initialization */
			DEBUG1("initWorld: Found %s class api\n", cls->className);
			memcpy(wCls, cls, sizeof(struct tfClass));
			wCls++; i++;
			if (i == MAX_TFCLASES) {
				DEBUG("Maximum target clases arrived! recomplile with more MAX_TFCLASES\n");
				break;
			}
		} else {
			DEBUG1("initWorld: Can't initialize %s class api!!!\n", cls->className);
		}
	}
	
	world->nApis = i;

	if (i == 0) {
		DEBUG("No situable driver found\n");
		return -1;
	}

	return i;
}


static struct Buffer *memToBuff(void *mem)
{
	return (struct Buffer *)((char *)mem - sizeof(struct _Buffer));
}

static struct InetConnection *allocInetConnection(struct World *wrld, SOCKET scli)
{
	struct InetConnection *conn;
	struct Buffer *buff;

	buff = allocBuff(wrld->buffAlloc, sizeof(struct InetConnection));
	if (buff == NULL) {
		DEBUG("allocInetConnection: can't allocate connection!\n");
		return NULL;
	}

	conn = (struct InetConnection *)buff->data;

	conn->dataDigest = 0;
	conn->headerDigest = 0;
	conn->headerSize = sizeof(struct bhsBase);

	conn->initiatorMaxRcvDataSegment = 8192;
	conn->pending = NULL;
	conn->wpending = NULL;
	conn->pndTask = NULL;

	memset(&conn->readOP, 0, sizeof(struct ReadWriteOP) * 2); /* both readOP & writeOP */

	conn->sending = NULL;
	conn->sockNeedToWrite = 0;
	conn->world = wrld;

	counterInit(&conn->recvStatSN, 0);
	conn->statSN = 0;
 /* conn->targetInfo = NULL; */
	conn->scli = scli;
	return conn;
}

static struct Session *allocSession(struct InetConnection *ic)
{
	struct Session *ses;
	struct AllocParam prm[SESSION_BUFFER_CLASSES] = SESSION_BUFFER_INITIALIZATOR;

	ses = (struct Session *)allocBuff(ic->world->buffAlloc, sizeof(struct Session))->data;
	if (ses == NULL) {
		DEBUG("allocSession: can't allocate session!\n");
		return NULL;
	}
	
	ses->list.next = ses->list.prev = (struct DCirList *)ses;

	ses->nConnections = 1;
	ses->connections = ic;
	ses->connections->member = ses;

	bufferAllocatorInit(ses->buffAlloc, prm, sizeof(prm) / sizeof(prm[0]));

	createStaticList(ses->taskPool, sizeof(struct Task), TASK_LIST);
	createStaticList(ses->resp, sizeof(struct ResponseTask), TASK_LIST);
	createStaticList(ses->pdus, sizeof(struct PDUList), PDU_LIST);

	ses->notAllRecived = NULL;
	ses->tasks = NULL;
	ses->immediateTasks = NULL;
	ses->resTasks = NULL;
	ses->sended = NULL;

	ses->freeTasks = ses->taskPool;
	ses->freeResTasks = ses->resp;
	ses->freePDUs = ses->pdus;

	ses->nFreeTasks = TASK_LIST;
	ses->nFreeResTasks = TASK_LIST;
	ses->nFreePDUs = PDU_LIST;

	counterInit(&ses->cmdSN, 0);
	ses->connChanged = 0;

	ses->perfImTasks = NULL;
	ses->perfTasks = NULL;

	ses->minMaxRcvDataSegment = 8192;
	ses->targetTaskTag = 0;
	
	ses->errorRecoveryLevel = 0;

	FD_ZERO(&ses->rop);
	FD_ZERO(&ses->wop);

	ses->bytes_read = 0;
	ses->bytes_write = 0;

	ses->pdus_recv = 0;
	ses->pdus_sent = 0;
	return ses;
}

static struct PDUList *allocParamPDU(struct AuthProc *ap)
{
	struct PDUList *tmp = (struct PDUList *)allocNode((struct DCirList**)&ap->freePDUs);
	if (tmp != NULL) {
		ap->nFreePDUs--;
	}
	return tmp;
}
/*
static void freeParamPDU(struct PDUList *node, struct AuthProc *ap)
{
	ap->nFreePDUs++;
	removeNode((struct DCirList*)node, (struct DCirList**)&ap->freePDUs);
}
*/


/* FIXME Bogus allocating of unique session ID*/
int allocTSIH(struct World *wrld)
{
	return 0x0778;
}

void parseParamClean(struct ParseParams *pp)
{
	lockWorld(pp->ic->world);

	freeBuff(pp->inBuff);
	freeBuff(pp->outBuff);

}

struct loginStruct {
	struct InetConnection *ic;
	struct PDUList *firstPDU;
	struct Param *params;
	uint32_t statSN;
	uint32_t cmdSN;
};

/***************************************************************************/

int doLogin(struct loginStruct *ls)
{
	struct InetConnection *ic = ls->ic;
	struct PDUList *firstPDU = ls->firstPDU;
	struct AuthProc ap;
	struct ParseParams pp;
	struct bhsRequest *req = (struct bhsRequest *)&firstPDU->pdu.bhs;
	struct bhsResponse *rsp;
	struct timeval tv = {5, 0};
	int res;

	if (BHS_ISSET(req->flags, BHS_LOGIN_C_BIT)) {
		DEBUG("doLogin: Continue bit in first PDU not supported!\n");
		
		ls->params = NULL;

		/*releaseWorld(&wrld);*/
		return -1;
	}

	ic->targetMaxRcvDataSegment = MAX_SEG_LENGTH;

	pp.inBuff = allocBuff(wrld.buffAlloc, DEF_MAX_RCV_SEG_LENGTH);
	pp.outBuff = allocBuff(wrld.buffAlloc, DEF_MAX_RCV_SEG_LENGTH);
	ls->params = pp.param = (struct Param *)allocBuff(wrld.buffAlloc, TOTAL_PARAMS_LEN)->data;

	releaseWorld(&wrld);

	initDefParams(pp.param);
	createStaticList(ap.freePDU, sizeof(struct PDUList), MAX_AUTH_PDU);
	ap.nFreePDUs = MAX_AUTH_PDU;
	ap.freePDUs = ap.freePDU;
	
	ap.statSN = 0;
	ap.availAuthTypes = 0;
	ap.cmdSN = req->cmdSN;
	ap.CSG = (req->flags & BHS_LOGIN_CSG_MSK) >> 2;
	ap.NSG = req->flags & BHS_LOGIN_NSG_MSK;
	ap.transit = BHS_ISSET(req->flags, BHS_LOGIN_T_BIT);
	ap.initiatorTaskTag = req->initiatorTaskTag;
	ap.CID = req->l.connectionID;
	ap.firstPDU = 1;
	ap.pending = NULL;
	ap.wpending = allocParamPDU(&ap);
	rsp = (struct bhsResponse *)&ap.wpending->pdu.bhs;

	pp.ap = &ap;
	pp.ic = ic;

	firstPDU->pdu.dataSeg = pp.inBuff;
	firstPDU->pdu.data = pp.inBuff->data;

	res = reciveFirstData(ic->scli, &firstPDU->pdu, &tv);
	if (res < 0) {
		/* FIXME! Free PP */
		parseParamClean(&pp);
		return -1;
	}

	if ((ap.NSG == 3) && (ap.transit)) {
		/*addParam(pp.outBuff, ISPN_AUTHMETHOD, ISPN_AM_NONE);*/
		addParamNum(pp.outBuff, ISPN_MAXRECVDATASEGMENTLENGTH, MAX_SEG_LENGTH);
		rsp->isid_tsih.tsih = allocTSIH(ic->world);	
	}

	res = parsePDUParam(&pp);

	rsp->initiatorTaskTag = ap.initiatorTaskTag;
	rsp->versionMax = 0;
	rsp->versionActive = 0;
	rsp->totalAHSLength = 0;
	rsp->cmd = BHS_OPCODE_LOGIN_RESPONSE;

	rsp->isid_tsih.TA = req->isid_tsih.TA;
	rsp->isid_tsih.B = req->isid_tsih.B;
	rsp->isid_tsih.C = req->isid_tsih.C;
	rsp->isid_tsih.D = req->isid_tsih.D;
	rsp->isid_tsih.tsih = req->isid_tsih.tsih;

	if ((ap.CSG == 1) && (ap.NSG == 3)) { /* Already in None authentification */
		ap.availAuthTypes = ISPN_AM_NONE_MASK;	
	}

	/* Selecting Auth Method */
	if ((res == -1) || 
		(((ap.availAuthTypes & ISPN_AM_NONE_MASK) == 0) && (ap.CSG == 0))) {
		if ((pp.param[ISP_SESSIONTYPE].ivalue == ISP_ST_NORMAL) && 
			(pp.param[ISP_TARGETNAME].pvalue == NULL))  {

			rsp->s.statusClass = AUTH_STATCLASS_INITIATOR_ERROR;
			rsp->s.statusDetail = 0x03;			
		} else {
		/* we don't support anothe type of auth */
		
			rsp->s.statusClass = AUTH_STATCLASS_INITIATOR_ERROR;
			rsp->s.statusDetail = 0x01;
		}
		rsp->flags = 0;
		ap.wpending->pdu.dataSegmentLength = 0;

		sendFirstPDU(ic->scli, &ap.wpending->pdu, &tv);
		/* FIXME! Free PP */
		parseParamClean(&pp);
		return -1;
	}


	if (pp.param[ISP_SESSIONTYPE].ivalue == ISP_ST_NORMAL) {
		/* cheking for correct params */

		/* OpenISCS bug??? */
/*		if ((pp.param[ISP_TARGETNAME].pvalue == NULL) && 
			(pp.param[ISP_INITIATORNAME].pvalue == NULL)) {
			pp.param[ISP_SESSIONTYPE].ivalue = ISP_ST_DISCOVER;
			pp.param[ISP_SESSIONTYPE].tvalue = ISPN_ST_DISCOVER;
		}
*/
	}
	
	rsp->flags = ap.NSG | (ap.CSG << 2) | (ap.transit * BHS_LOGIN_T_BIT);
	rsp->s.statusClass = 0;
	rsp->s.statusDetail = 0;
	rsp->statSN = ap.statSN++;
	rsp->expCmdSN = ap.cmdSN + 1;
	rsp->maxCmdSN = ap.cmdSN + ap.nFreePDUs;
	
	rsp->isid_tsih.tsih = 0;//allocTSIH(ic->world);	

	if ( !((ap.NSG == 3) && (ap.transit))) {
		addParam(pp.outBuff, ISPN_AUTHMETHOD, ISPN_AM_NONE);
	}
/*
	if ((ap.NSG == 3) && (ap.transit)) {
		addParamNum(pp.outBuff, ISPN_MAXRECVDATASEGMENTLENGTH, 131072);
		rsp->isid_tsih.tsih = allocTSIH(ic->world);	
	}
*/
	ap.wpending->pdu.dataSegmentLength = pp.outBuff->length;
	ap.wpending->pdu.dataSeg = pp.outBuff;
	ap.wpending->pdu.data = pp.outBuff->data;


	res = sendFirstPDU(ic->scli, &ap.wpending->pdu, &tv);
	if (res < 0) {
		/* FIXME! Free PP */
		parseParamClean(&pp);
		return -1;
	}

	if (ap.NSG == 3) {
		goto end_none_auth;
	}

	ap.pending = allocParamPDU(&ap);
	ap.pending->pdu.dataSeg = pp.inBuff;
	ap.pending->pdu.data = pp.inBuff->data;
	ap.firstPDU = 0;
	req = (struct bhsRequest *)&ap.pending->pdu.bhs;


	for (;(ap.NSG != 3) || (ap.transit == 0);) {
		pp.outBuff->length = 0;
		pp.inBuff->length = 0;

		res = reciveFirstPDU(ic->scli, &ap.pending->pdu, &tv);
		if (res < 0) {
			/* FIXME! Free PP */
			parseParamClean(&pp);
			return -1;
		}

		if ((req->cmd & BHS_OPCODE_MASK) != BHS_OPCODE_LOGIN_REQUEST) {
			/* Not Login requet PDU in login phase */
			rsp->s.statusClass = AUTH_STATCLASS_INITIATOR_ERROR;
			rsp->s.statusDetail = 0x0b;
			rsp->flags = 0;
			ap.wpending->pdu.dataSegmentLength = 0;
			
			sendFirstPDU(ic->scli, &ap.wpending->pdu, &tv);
			/* FIXME! Free PP */
			parseParamClean(&pp);
			return -1;			
		}

		ap.CSG = (req->flags & BHS_LOGIN_CSG_MSK) >> 2;
		ap.NSG = req->flags & BHS_LOGIN_NSG_MSK;
		ap.transit = BHS_ISSET(req->flags, BHS_LOGIN_T_BIT);
		ap.initiatorTaskTag = req->initiatorTaskTag;

		if ((ap.NSG == 3) && (ap.transit)) {
			rsp->isid_tsih.tsih = allocTSIH(ic->world);			
		} else {
			rsp->isid_tsih.tsih = 0;
		}
		
		res = parsePDUParam(&pp);
		if (res < 0) {
			DEBUG("doLogin: parsePDUParam return < 0\n");
		}
		
		rsp->cmd = BHS_OPCODE_LOGIN_RESPONSE;
		rsp->versionMax = 0;
		rsp->versionActive = 0;
		rsp->flags = ap.NSG | (ap.CSG << 2) | (ap.transit * BHS_LOGIN_T_BIT);
		rsp->s.statusClass = 0;
		rsp->s.statusDetail = 0;
		rsp->statSN = ap.statSN++;
		rsp->expCmdSN = ap.cmdSN + 1;
		rsp->maxCmdSN = ap.cmdSN + ap.nFreePDUs;
		rsp->initiatorTaskTag = ap.initiatorTaskTag;
		rsp->totalAHSLength = 0;

		addParamNum(pp.outBuff, ISPN_MAXRECVDATASEGMENTLENGTH, MAX_SEG_LENGTH);

		ap.wpending->pdu.dataSegmentLength = pp.outBuff->length;

		res = sendFirstPDU(ic->scli, &ap.wpending->pdu, &tv);
		if (res < 0) {
			/* FIXME! Free PP */
			parseParamClean(&pp);
			return -1;
		}		
	}

end_none_auth:
	ls->statSN = ap.statSN;
	ls->cmdSN = ap.cmdSN;

	if (pp.param[ISP_SESSIONTYPE].avalue == ISP_ST_NORMAL) {		
		lockWorld(pp.ic->world);
		return 0;
	}

	/*******************************/
	/* Perfoming Discovery Session */
	/*******************************/
	for (;;) {
		pp.outBuff->length = 0;
		pp.inBuff->length = 0;

		res = reciveFirstPDU(ic->scli, &ap.pending->pdu, &tv);
		if (res < 0) {
			/* FIXME! Free PP */
			parseParamClean(&pp);
			return -1;
		}

		res = (req->cmd & BHS_OPCODE_MASK);
		switch (res) {
		case BHS_OPCODE_TEXT_REQUEST:
		/*	if (BHS_NOTSET(req->cmd, BHS_FLAG_FINAL) || BHS_ISSET(req->cmd, BHS_FLAG_CONTINUE)) {
				DEBUG("doLogin: Can't operand with fragment text request in Discovery session yet!\n");	
				parseParamClean(&pp);
				return -1;
			}
			*/
			rsp->cmd = BHS_OPCODE_TEXT_RESPONSE;
			rsp->initiatorTaskTag = req->initiatorTaskTag;
			rsp->totalAHSLength = 0;
			rsp->flags = BHS_FLAG_FINAL;
			rsp->statSN = ap.statSN++;
			rsp->expCmdSN = req->cmdSN + 1;
			rsp->maxCmdSN = req->cmdSN + pp.ap->nFreePDUs;
			rsp->targetTransferTag = 0xffffffff;

			res = parsePDUParam(&pp);
			if (res < 0) {
				DEBUG("doLogin: parsePDUParam return < 0 in Discovery Session\n");
			}
			break;
		case BHS_OPCODE_LOGOUT_REQUEST:
			rsp->cmd = BHS_OPCODE_LOGOUT_RESPONSE;
			rsp->initiatorTaskTag = req->initiatorTaskTag;
			rsp->totalAHSLength = 0;
			rsp->flags = BHS_FLAG_FINAL;
			rsp->statSN = ap.statSN++;
			rsp->expCmdSN = req->cmdSN + 1;
			rsp->maxCmdSN = req->cmdSN + pp.ap->nFreePDUs;
			rsp->t.time2Wait = pp.param[ISP_DEFAULTTIME2WAIT].ivalue;
			rsp->t.time2Retain = pp.param[ISP_DEFAULTTIME2RETAIN].ivalue;
			rsp->response = 0;
			break;
		default:
			DEBUG1("doLogin: Unknown PDU in Discovery session: %02x!\n", res);
			parseParamClean(&pp);
			return -1;
		}

		ap.wpending->pdu.dataSegmentLength = pp.outBuff->length;
		res = sendFirstPDU(ic->scli, &ap.wpending->pdu, &tv);
		if (res < 0) {
			/* FIXME! Free PP */
			parseParamClean(&pp);
			return -1;
		}
		if (rsp->cmd == BHS_OPCODE_LOGOUT_RESPONSE) {
			parseParamClean(&pp);
			return 1;
		}
	}

}

/*
static int initSync(struct Session *ses)
{
#ifndef _SYNC_SCSI
	ses->sSync = socket(AF_INET, SOCK_DGRAM, 0);
#endif

	return 0;
}
*/

/* Entering in BLOCKED world */
static int selectTfAPI(struct Session *ses)
{
	struct confElement *elem = (struct confElement *)ses->params[ISP_TARGETNAME].pvalue;
	struct World *wrld = ses->connections->world;
	int i;
	
	for (i = 0; i < wrld->nApis; i++) {
		if (strcmp(wrld->apis[i].className, elem->value) == 0) {

			memcpy(&ses->cclass, &wrld->apis[i], sizeof(struct tfClass));
			ses->tfHandle = ses->cclass.tfAttach(ses->cclass.handle, ses);
			if (ses->tfHandle == NULL) {
				DEBUG("selectTfAPI: Can't attach target!\n");
				return -1;
			}

			return 0;
		}
	}

	return -1;

}

int doClient2(SOCKET scli)
{
	struct PDUList firstPDU;
	int res;//, i;
	int newses = 0;

	struct Session *ses;
	struct Session *tses;
	struct InetConnection *ic;
	struct loginStruct ls;

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;


	res = reciveFirstBHS(scli, &firstPDU.pdu, &tv);
	if (res != 0) {
		/* hard error */
		return -1;
	}
	res = firstPDU.pdu.bhs.cmd & BHS_OPCODE_MASK;
	if (res != BHS_OPCODE_LOGIN_REQUEST) {
		DEBUG("doClient2: first PDU isn't Login request\n");
		return -1;
	}


	/* Checking for new session, new connection or session recovery */
	
	/* at these time only new session is valid */
	lockWorld(&wrld);
	ses = wrld.sesions;

	if (ses != NULL) {
		tses = ses;
		do {
			res = memcmp(&((struct bhsRequest *)&firstPDU.pdu.bhs)->isid_tsih,
				&tses->sessionID, sizeof(struct ISID_TSIH));
			if (res == 0) {
				DEBUG("EE: Multi connections not supported yet\n");
				newses = -1;
			}
			tses = (struct Session *)tses->list.next;
		} while ( tses != ses);
	}

	if (newses == 0) {
		ic = allocInetConnection(&wrld, scli);
	} else {
		DEBUG("Hard error! coling connection\n");
		releaseWorld(&wrld);
		return -1;
	}

	ls.ic = ic;
	ls.firstPDU = &firstPDU;
	
	res = doLogin(&ls);
	if (res == -1) {
		freeBuff(memToBuff(ls.params));
		freeBuff(memToBuff(ic));
		releaseWorld(&wrld);

		DEBUG("doLogin: closing connection\n");
		shutdown(scli, 2);
		return 0;
	} else if (res == 1) {
		/* success discovery session */
		freeBuff(memToBuff(ls.params));
		freeBuff(memToBuff(ic));
		releaseWorld(&wrld);

		DEBUG("-- Discovery session successfuly closed\n");
		shutdown(scli, 2);
		return 0;
	}

	/* ses allocating */
	ses = allocSession(ic);
	ses->params = ls.params;

	/*********************************************/
	/* Selecting device class API */
	res = selectTfAPI(ses);
	if (res == 0) {
		addNode((struct DCirList *)ses, (struct DCirList **)&wrld.sesions);
		wrld.nSessions ++;
	}

	releaseWorld(&wrld);
	
	if (res < 0) {
		DEBUG("Error in initializing TF\n");	
		/* FIXME: Closing sessino with error message */
	} else {
		ic->statSN = ls.statSN;
		counterInit(&ses->cmdSN, ls.cmdSN);
		ses->errorRecoveryLevel = 0;
		
		if (ic->headerDigest == ISP_DIG_CRC32C) {
			ic->headerSize += 4;
		}
		
		ses->minMaxRcvDataSegment = ic->initiatorMaxRcvDataSegment;

		/* initSync(ses); */
		
		DEBUG("-- Perfoming normal session\n");
		doConnection(ses->connections);

		/* FIXME: Session reasigment!! */
		removeNode((struct DCirList *)ses, (struct DCirList **)&wrld.sesions);
		wrld.nSessions --;

		ses->cclass.tfDetach(ses->cclass.handle, ses->tfHandle);
	}
	
	lockWorld(&wrld);
	bufferAllocatorClean(ses->buffAlloc);
	freeBuff(memToBuff(ses->params));

	freeBuff(memToBuff(ses));
	freeBuff(memToBuff(ic));
	releaseWorld(&wrld);
	
	shutdown(scli, 2);

	return 0;
}


/*
#define DUMP_RS
*/

int doConnection(struct InetConnection *conn)
{
	struct Session *ses = conn->member;
	int res;
	int max_fd;

#ifndef _SYNC_SCSI
	struct sockaddr_in addr;
	int value;
	int len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SYNC_PORT);
	addr.sin_addr.s_addr = htonl(0x7f000001);
	res = bind(ses->sSync, (struct sockaddr*)&addr, sizeof(addr));
#endif
	max_fd = conn->scli + 1;
	for (;;) {
		FD_SET(conn->scli, &ses->rop);
#ifndef _SYNC_SCSI	
		FD_SET(ses->sSync, &ses->rop);
#endif
		FD_ZERO(&ses->wop);

		if (conn->sockNeedToWrite == 1) {
			FD_SET(conn->scli, &ses->wop);	
		}
		res = select(max_fd, &ses->rop, &ses->wop, NULL, NULL);
#ifdef DUMP_RS
		DEBUG("Selecting\n");
#endif
		if (FD_ISSET(conn->scli, &ses->wop)) {
			if (res > 0) { /* some sockets are ready */
				res = sendPDU(conn);
				/* if res == 0 need more data */
				if (res > 0) {
					/* reciving complete */	
#ifdef DUMP_RS					
					DEBUG("Sending complete\n");
#endif
				} else if (res == 0) {
					/* continue */
					continue;
				} else if (res == RWOP_SHUTDOWN) {
					DEBUG3("Readed: "LONG_FORMAT" bytes (%.2f Mb), "LONG_FORMAT" PDUs\n", ses->bytes_read,
						((double)ses->bytes_read / (1024*1024)), ses->pdus_recv);
					DEBUG3("Writed: "LONG_FORMAT" bytes (%.2f Mb), "LONG_FORMAT" PDUs\n", ses->bytes_write,
						((double)ses->bytes_write / (1024.0*1024.0)), ses->pdus_sent);
					DEBUG("Closing session\n");
					/*close(conn->scli);*/
					return 0;
				} else {
					/* error! */
					return -1;
				}

			} else {
				DEBUG("Select returned <= 0\n");
				return -1;
			}

		} else if (FD_ISSET(conn->scli, &ses->rop)) {
			if (res > 0) { /* some sockets are ready */
				res = recivePDU(conn);
				/* if res == 0 need more data */
				if (res == 1) {
					/* reciving complete */		
#ifdef DUMP_RS					
					DEBUG("Reciving complete\n");
#endif
				} else if (res == 0) {
					/* continue */
					continue;
				} else {
					/* error! */
					return -1;
				}
			} else {
				DEBUG("Select returned <= 0\n");
				return -1;
			}
		} 
#ifndef _SYNC_SCSI		
		else if (FD_ISSET(ses->sSync, &ses->rop)) {
			
			res = recvfrom(ses->sSync, (char*)&value, sizeof(int), 0, (struct sockaddr *)&addr, &len);
			if (res != sizeof(int)) {
				DEBUG1("Sync socket recive len: %d\n", res);				
				return -1;
			} else {
				sesQueueTask(ses, &ses->taskPool[value]);
			}

		}
#endif
	}

	return 0;
}



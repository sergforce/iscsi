#include "sock.h"
#include "iscsi.h"
#include "tf.h"
#include "dcirlist.h"
#include "iscsi_mem.h"
#include "iscsi_param.h"
#include "conf_reader.h"
#include "iscsi_session.h"
#include "contrib/crc32.h"

#include <stdio.h>
#include <stdlib.h>

#include "debug/iscsi_debug.h"


static uint32_t allocTargetTaskTag(struct Session *ses);
int sesTaskOp(struct Session *ses);
static int logout(struct Session *ses, struct Task *tsk);

static int translateTf(struct Session *ses, struct Task *tsk);

/*
#define DUMP_PDU
#define DUMP_LOGIN_PDUS
*/

#define MIN(a,b) ((a) > (b) ? (b) : (a))

/***********************************************************************/
/*                        Counter operations                           */
/***********************************************************************/

void counterInit(struct Counter *cnt, uint32_t value)
{
	cnt->count = value;
	cnt->mask = 0;
}

int counterAddValue(struct Counter *cnt, uint32_t value)
{
	int k = value - cnt->count;
	counter_t msk;

	if ((cnt->mask == 0) && (k == 1)) {
		cnt->count++;
		return COUNTER_IN_ORDER;
	}

	if ((k > COUNTER_BITS)||(k < 0)) {
		DEBUG("counterAddValue: more losses!\n");
		return COUNTER_ERROR;
	}

	if (k == 0) {
		return COUNTER_DUP;
	}

	if (k != 1) {
		cnt->mask |= COUNTER_MASK >> (k - 1);
		return COUNTER_OUT_OF_ORDER;
	} else {
		for (msk = (cnt->mask << 1), k = 1; (msk & COUNTER_MASK) != 0; k++) {
			msk <<= 1;
		}
		cnt->mask = msk;
		cnt->count += k;
		return COUNTER_REPAIRED;
	}
}

uint32_t counterGetValue(struct Counter *cnt)
{
	return cnt->count;
}

uint32_t counterGetMaxValue(struct Counter *cnt)
{
	return (cnt->count + (COUNTER_BITS - 1));
}

int sIsSmaller(uint32_t s1, uint32_t s2)
{
	return (((s1 < s2) && ((s2 - s1) < ((2^(SERIAL_BITS - 1)) - 1)))  ||
			((s1 > s2) && ((s2 - s1) > ((2^(SERIAL_BITS - 1)) - 1)))) ? 1 : 0;
}

int sIsLarger(uint32_t s1, uint32_t s2)
{
	return (((s1 < s2) && ((s2 - s1) > ((2^(SERIAL_BITS - 1)) - 1)))  ||
			((s1 > s2) && ((s2 - s1) < ((2^(SERIAL_BITS - 1)) - 1)))) ? 1 : 0;
}

/***********************************************************************/
/*                        Socket operations                            */
/***********************************************************************/

int reciveOP(SOCKET s, struct ReadWriteOP *readOP)
{
	int res = recv(s, (char *)(readOP->buffer + readOP->perfomedLen), readOP->expectedLen - readOP->perfomedLen, 0);
	if (res == -1) { /* connection reset */
		return RWOP_ERROR;
	} else if (res == 0) {		
		return RWOP_SHUTDOWN;
	} else {
		readOP->perfomedLen += res;		

		if (readOP->perfomedLen != readOP->expectedLen) {
			/* not all */
			return RWOP_TRANS_NOT_ALL;
		} else {
			/* ready to next stage */
			if ((readOP->stage & RWOP_CRCFLAG) == RWOP_CRCFLAG) {
				res = recv(s, (char *)&readOP->dataCRC, sizeof(uint32_t), 0);
				if (res == -1) { /* connection reset */
					return RWOP_ERROR;
				} else if (res == 0) {		
					return RWOP_SHUTDOWN;
				} else if (res == 4) {		
					readOP->stage = (readOP->stage & RWOP_STAGEMASK) + 1;
					return RWOP_TRANS_OK;
				} else {
					DEBUG("reciveOP: error during reciving CRC\n");
					return RWOP_ERROR;
				}
			}
			readOP->stage++;
			return RWOP_TRANS_OK;
		}
	}	
}


int sendOP(SOCKET s, struct ReadWriteOP *writeOP)
{
	int res = send(s, (char *)(writeOP->buffer + writeOP->perfomedLen), writeOP->expectedLen - writeOP->perfomedLen, 0);
	if (res == -1) { /* connection reset */
		return RWOP_ERROR;
	} else if (res == 0) {		
		return RWOP_SHUTDOWN;
	} else {
		writeOP->perfomedLen += res;		

		if (writeOP->perfomedLen != writeOP->expectedLen) {
			/* not all */
			return RWOP_TRANS_NOT_ALL;
		} else {
			if ((writeOP->stage & RWOP_CRCFLAG) == RWOP_CRCFLAG) {
				res = send(s, (char *)&writeOP->dataCRC, sizeof(uint32_t), 0);
				if (res == -1) { /* connection reset */
					return RWOP_ERROR;
				} else if (res == 0) {		
					return RWOP_SHUTDOWN;
				} else if (res == 4) {		
					writeOP->stage = (writeOP->stage & RWOP_STAGEMASK) + 1;
					return RWOP_TRANS_OK;
				} else {
					DEBUG("sendOP: error during reciving CRC\n");
					return RWOP_ERROR;
				}
			}
			/* ready to next stage */
			writeOP->stage++;
			return RWOP_TRANS_OK;
		}
	}	
}


/***********************************************************************/
/*                         Tasks operations                            */
/***********************************************************************/


int rejectPDU(struct InetConnection *conn, struct Task *bad, uint8_t problem, uint32_t dataSN);
int cleanAckStat(struct Session *ses, uint32_t expStatSN);

struct Task *findTask(struct Session *ses, uint32_t initiatorTaskTag)
{
	struct Task *tsk = ses->notAllRecived;
	if ((tsk == NULL) || (initiatorTaskTag == 0xffffffff)) {
		return NULL;
	}

	do {
		if (tsk->initiatorTaskTag == initiatorTaskTag) {
			return tsk;
		}

		tsk = (struct Task *)tsk->list.next;
	} while (tsk != ses->notAllRecived);

	return NULL; /* not found */
}

struct ResponseTask *findResponseTask(struct Session *ses, uint32_t targetTransferTag)
{
	struct ResponseTask *tsk = ses->sended;
	if ((tsk == NULL) || (targetTransferTag == 0xffffffff))  {
		return NULL;
	}

	do {
		if (tsk->targetTransferTag == targetTransferTag) {
			return tsk;
		}

		tsk = (struct ResponseTask *)tsk->list.next;
	} while (tsk != ses->sended);

	return NULL; /* not found */
}


static struct Task *allocTask(struct Session *ses)
{
	struct Task *tsk = ((struct Task *)allocNode((struct DCirList **)&ses->freeTasks));
	if (tsk != NULL) {
		ses->nFreeTasks--;	
	}
	return tsk;
}

static void freeTask(struct Session *ses, struct Task *tsk)
{
	ses->nFreeTasks++;
	addNode((struct DCirList *)tsk, (struct DCirList **)&ses->freeTasks);
}


static struct ResponseTask *allocResponseTask(struct Session *ses)
{
	struct ResponseTask *rtsk = ((struct ResponseTask *)allocNode((struct DCirList **)&ses->freeResTasks));
	if (rtsk != NULL) {
		ses->nFreeResTasks--;	
	}
	return rtsk;
}

static void freeResponseTask(struct Session *ses, struct ResponseTask *rtsk)
{
	ses->nFreeResTasks++;
	addNode((struct DCirList *)rtsk, (struct DCirList **)&ses->freeResTasks);
}


static struct PDUList *allocPDU(struct Session *ses)
{
	struct PDUList *pdu = (struct PDUList *)allocNode((struct DCirList **)&ses->freePDUs);
	if (pdu != NULL) {
		ses->nFreePDUs--;
	}
	return pdu;
}

static void freePDU(struct Session *ses, struct PDUList *pdu)
{
	ses->nFreePDUs++;
	addNode((struct DCirList *)pdu, (struct DCirList **)&ses->freePDUs);
}

static unsigned min3(unsigned v1, unsigned v2, unsigned v3) 
{
	if (v1 < v2) {
		if (v3 < v1) {
			return v3;
		} else {
			return v1;
		}
	} else { /* v2 < v1 */
		if (v3 < v2) {
			return v3;
		} else {
			return v2;
		}
	}
}


static struct Task *getTask(struct InetConnection *conn, struct bhsRequest *req)
{
	struct Task *tsk = allocTask(conn->member);
	if (tsk == NULL) {
		return NULL;
	}		
	
	tsk->initiatorTaskTag = req->initiatorTaskTag;
	tsk->cmdSN = req->cmdSN;
	tsk->dataSegment = NULL;
	tsk->intialRes = NULL;
	tsk->pdus = conn->pending;
	
	tsk->lun = req->lunL;
		
	tsk->nPDU = 1;
	tsk->immediate = (((req->cmd & BHS_OPCODE_IMMEDIATE_BIT) == BHS_OPCODE_IMMEDIATE_BIT) &&
		((req->cmd & BHS_OPCODE_MASK) != BHS_OPCODE_SCSI_DATA_OUT) &&
		((req->cmd & BHS_OPCODE_MASK) != BHS_OPCODE_SNACK_REQUEST));
	
	tsk->nR2t = 0;
	tsk->r2tRes = NULL;
	tsk->dataSNr2tSN = 0;

	counterInit(&tsk->unsolDataSN, 0);
	return tsk;
}


struct Task *tfCmd2Task(struct tfCommand *cmd)
{
	return (struct Task *)(((char*)cmd) - (sizeof(struct Task) - sizeof(struct tfCommand)));
}


static void freePDUs(struct Session *ses, struct PDUList **pdus)
{
	struct PDUList *pdu;
	while (*pdus) {
		pdu = (struct PDUList *)allocNode((struct DCirList **)pdus);
		freePDU(ses, pdu);
	}
}

static void cleanupPendingPDU(struct InetConnection *conn)
{
	freePDU(conn->member, conn->pending);
	conn->readOP.stage = 0;
}

static void cleanupPendingTask(struct InetConnection *conn)
{
	if (removeNode((struct DCirList *)conn->pending, (struct DCirList **)&conn->pndTask->pdus) == NULL) {
		return;
	}
	freePDU(conn->member, conn->pending);

	if (conn->pndTask->pdus == NULL) {
	
		if (conn->pndTask->dataSegment != NULL) {
			freeBuff(conn->pndTask->dataSegment);
		}
	
		freeTask(conn->member, conn->pndTask);
	}
	conn->readOP.stage = 0;
}


int recivePDU(struct InetConnection *conn)
{
	struct ReadWriteOP *readOP = &conn->readOP;
	struct PDUList *pending = conn->pending;
	struct Task *tsk = NULL;
	struct ResponseTask *rtsk;
	struct Session *ses = conn->member;

	int res;
	uint32_t tmp, crc;
	uint32_t ahsLength;
	struct Buffer *buff;

	struct bhsRequest *req;

	if ((readOP->stage % 2) == 1) {/* phase not ended */
		if (readOP->stage == 5) {
			tsk = conn->pndTask;
		}
		res = reciveOP(conn->scli, readOP);
		if (res < 0) { /* error or connection error */
			if (readOP->stage < 5) {
				cleanupPendingPDU(conn);
			} else {
				cleanupPendingTask(conn);
			}
			return res;
		} else if (res == RWOP_TRANS_NOT_ALL) {
			return RWPDU_NOTALL;
		}
	}

	switch (readOP->stage) {
	case 0: /* intial state, preparing to read BHS */
		conn->pending = pending = allocPDU(ses);

		readOP->stage = 1;
		readOP->buffer = (uint8_t *)&pending->pdu.bhs;
		readOP->expectedLen = conn->headerSize;
		readOP->perfomedLen = 0;

		res = reciveOP(conn->scli, readOP);
		if (res < 0) { /* error or connection error */
			DEBUG("recivePDU: error during recive 0\n");
			cleanupPendingPDU(conn);
			return res;
		} else if (res == RWOP_TRANS_NOT_ALL) {
			return RWPDU_NOTALL;
		}

		ses->pdus_recv++;
		ses->bytes_read += readOP->expectedLen;

	case 2: /* reciving BHS complete, we need AHS? */
		ahsLength = pending->pdu.bhs.totalAHSLength;
		if (ahsLength != 0) {
			if (ahsLength != sizeof(struct AHS)) {
				DEBUG1("recivePDU: PDU with ahs of %d bytes\n", ahsLength);
				cleanupPendingPDU(conn);
				/* FIXME! reject this PDU */
				/*rejectPDU(conn, pending);*/
				return RWPDU_NOTALL;
			}
			
			readOP->stage = 3;
			readOP->buffer = (uint8_t *)&pending->pdu.ahs;
			readOP->expectedLen = sizeof(struct AHS);
			readOP->perfomedLen = 0;
		
			res = reciveOP(conn->scli, readOP);
			if (res < 0) { /* error or connection error */
				DEBUG("recivePDU: error during recive 2\n");
				cleanupPendingPDU(conn);
				return res;
			} else if (res == RWOP_TRANS_NOT_ALL) {
				return RWPDU_NOTALL;
			}

			ses->bytes_read += readOP->expectedLen;
		}
	case 4: /* Headers reciving complete, check CRC if needed */
		if (conn->headerDigest == ISP_DIG_CRC32C) {
			ahsLength = pending->pdu.bhs.totalAHSLength;
			if (ahsLength > 0) {				
				crc = pending->pdu.headerDigestWAHS;
			} else {
				crc = pending->pdu.headerDigest;
			}
				
			tmp = crc32c((uint8_t *)&pending->pdu.bhs, sizeof(struct bhsBase) + ahsLength);
			
			if (tmp != crc) { /* CRC don't match, drop PDU */
				DEBUG2("recivePDU: Header CRC Don't match! exp=0x%08x calc=0x%08x\n", crc, tmp);
				cleanupPendingPDU(conn);
				return RWPDU_NOTALL;
			}
		}
		/* Converting PDU order byte to this machine format */
		req = (struct bhsRequest *)&pending->pdu.bhs;

#ifdef LITTLE_ENDIAN
		tmp = ntohl(req->lun[0]);
		req->lun[0] = ntohl(req->lun[1]);
		req->lun[1] = tmp;
		req->initiatorTaskTag = ntohl(req->initiatorTaskTag);
		req->cmdSN = ntohl(req->cmdSN);
		req->expStatSN = ntohl(req->expStatSN);
#endif
		tmp = req->cmd & BHS_OPCODE_MASK;
		switch (tmp) {
		case BHS_OPCODE_SCSI_DATA_OUT:
		case BHS_OPCODE_SNACK_REQUEST:
			req->runLength = ntohl(req->runLength);
			req->begRun = ntohl(req->begRun);
		case BHS_OPCODE_SCSI_TASKMAN:
			req->expDataSN = ntohl(req->expDataSN);
			req->refCmdSN = ntohl(req->refCmdSN);			
		case BHS_OPCODE_NOPOUT:
		case BHS_OPCODE_SCSI_COMMAND:
		case BHS_OPCODE_LOGIN_REQUEST:
		case BHS_OPCODE_TEXT_REQUEST:
			req->refTaskTag = ntohl(req->refTaskTag);
			break;
		case BHS_OPCODE_LOGOUT_REQUEST:
			req->l.connectionID = ntohs(req->l.connectionID);
			break;
		default:
			/* BAD BHS opcode */
			DEBUG1("recivePDU: unexpected PDU opcode %d\n", req->cmd);
			cleanupPendingPDU(conn);
			/* FIXME! reject this PDU */
			return RWPDU_NOTALL;
		}

		res = 0; /* Task not fragmenred */
		if ((tmp == BHS_OPCODE_SCSI_COMMAND) || (tmp == BHS_OPCODE_SCSI_DATA_OUT)) {
			/* my be fragmented */
			/* res = req->flags & BHS_FLAG_FINAL; */
			tsk = findTask(ses, req->initiatorTaskTag);
			if (tsk == NULL) {
				tsk = getTask(conn, req);
				/***/
				if (tsk == NULL) {
					DEBUG("recivePDU: can't accocate task!\n");
					cleanupPendingPDU(conn);
					return RWPDU_NOTALL;
				}

				/*tsk->initiatorTaskTag = req->initiatorTaskTag;*/
				/*tsk->lun = req->lunL;*/
				/*counterInit(&tsk->unsolDataSN, 0);*/
			} else {
				res = 1; /* fragmented PDU */
				addNode((struct DCirList *)pending, (struct DCirList **)&tsk->pdus);
				tsk->nPDU++;
				DEBUG("recivePDU: found fragmented PDU\n");
			}
		} else {
			tsk = getTask(conn, req);
		}
		
		if (tsk == NULL) {
			/* FIXME! What's to do? */
			DEBUG("Task Didn't create!\n");
			return -3;
		}

		conn->pndTask = tsk;
		/* we need more data? */
		pending->pdu.dataSegmentLength = UINT24_TO_UINT(req->dataSegmentLength);
		if (pending->pdu.dataSegmentLength > 0) {			
			if (pending->pdu.dataSegmentLength > conn->targetMaxRcvDataSegment) {
				
				DEBUG2("recivePDU: CRITICAL!! data segment then target MaxRcvSegment: 0x%08x of MAXIMUM 0x%08x\n",
					pending->pdu.dataSegmentLength, conn->targetMaxRcvDataSegment);
			}

			readOP->stage = 5;
			readOP->expectedLen = ((4 - (pending->pdu.dataSegmentLength % 4)) % 4)
				+ pending->pdu.dataSegmentLength;
			readOP->perfomedLen = 0;
			if (conn->dataDigest == ISP_DIG_CRC32C) {
				readOP->stage |= RWOP_CRCFLAG;
			}

			buff = tsk->dataSegment;

			if ((req->cmd & BHS_OPCODE_MASK) ==	BHS_OPCODE_SCSI_COMMAND) {
				if (res == 1) {
					DEBUG("recivePDU: CRITIACAL!!! SCSI_COMMAND with existing ITT!\n");
					cleanupPendingTask(conn);
					return RWPDU_SES_ERROR;
				} else {
					/*DEBUG2("recivePDU: DataOUT %08x (%08x)\n", pending->pdu.dataSegmentLength,
						req->expDataTL);*/

					/* allocating FirtBurstLength */
					
					/*if (req->expDataTL < ses->params[ISP_MAXBURSTLENGTH].ivalue) {

					}*/
					if (req->expDataTL > (uint32_t)ses->params[ISP_MAXBURSTLENGTH].ivalue) {
						DEBUG2("recivePDU: Data more then MaxBurstLength: 0x%08x of MAXIMUM 0x%08x\n",
							req->expDataTL, ses->params[ISP_MAXBURSTLENGTH].ivalue);
					}
					if (req->expDataTL > conn->targetMaxRcvDataSegment) {
						DEBUG2("recivePDU: Data more then target MaxRcvSegment: 0x%08x of MAXIMUM 0x%08x\n",
							req->expDataTL, conn->targetMaxRcvDataSegment);
					}
					
					buff = allocBuff(ses->buffAlloc, 
							ses->params[ISP_MAXBURSTLENGTH].ivalue);

					if (buff == NULL) {
						/* FIXME! no memory */
						DEBUG("recivePDU: CRITIACAL!!! Task buffer didn't create!\n");
						cleanupPendingTask(conn);
						return RWPDU_SES_ERROR;
					}
					tsk->dataSegment = buff;
					buff->length = pending->pdu.dataSegmentLength;
					//////////////////////////////////////////////////
					pending->pdu.data = readOP->buffer = buff->data;

				}
			} else if ((req->cmd & BHS_OPCODE_MASK) == BHS_OPCODE_SCSI_DATA_OUT) {
				/* select buffer with appropirate size */
				/* for Data-Out command all fragments must be defragmented */
				//DEBUG1("recivePDU: SCSI Dataout defragmenting, offset=%08x\n", req->buffOffset);
				//dumpPDU(&pending->pdu);

				rtsk = findResponseTask(ses, req->targetTransferTag);

				if (buff == NULL) {
					if (rtsk == NULL) {
						/* tag not found what to do? */
						/* act as session error! */
						DEBUG("recivePDU: CRITIACAL!!! SCSI_DATA_OUT with unrecognaized TTT and ITT!\n");
						cleanupPendingTask(conn);
						return RWPDU_SES_ERROR;

					} else {				
						buff = allocBuff(ses->buffAlloc, 
							((struct bhsResponse *)&rtsk->pdus->pdu.bhs)->desiredDataTL);
					}

					if (buff == NULL) {
						/* FIXME! no memory */
						DEBUG("recivePDU: CRITIACAL!!! Task buffer didn't create!\n");
						cleanupPendingTask(conn);
						return RWPDU_SES_ERROR;
					}
				}

				tsk->dataSegment = buff;

				if (rtsk != NULL) {					
					counterAddValue(&rtsk->dataSN, req->dataSN);
				} else {
					counterAddValue(&tsk->unsolDataSN,  req->dataSN);
				}

				if (req->buffOffset + readOP->expectedLen > buff->maxLength) {
					/* buffer overflow! reject PDU? */
					DEBUG("recivePDU: buffer overflow!\n");
					return RWPDU_NOTALL;
				}
				/* fixme data digest */
				//DEBUG1("recivePDU: defragmenting, offset=%08x\n", req->buffOffset);

				pending->pdu.data = readOP->buffer = buff->data + req->buffOffset;
			} else if (buff == NULL) {	
				/* FIXME!!! */
				buff = allocBuff(ses->buffAlloc, readOP->expectedLen);
				if (buff == NULL) {
					/* FIXME! no memory */
					cleanupPendingTask(conn);
					return RWPDU_NOTALL;
				}
				tsk->dataSegment = buff;

				pending->pdu.data = readOP->buffer = buff->data;
			}

			res = reciveOP(conn->scli, readOP);
			if (res < 0) { /* error or connection error */
				cleanupPendingTask(conn);
				return res;
			} else if (res == RWOP_TRANS_NOT_ALL) {
				return RWPDU_NOTALL;
			}
			
			ses->bytes_read += readOP->expectedLen;

		} else { /* pending->pdu.dataSegmentLength == 0 */
			pending->pdu.data = NULL;			
		}
		
	case 6:	/* all work done */
		readOP->stage = 0;
	
		/* cheking numbering */
		req = (struct bhsRequest *)&pending->pdu.bhs;

		if ((pending->pdu.dataSegmentLength > 0) && (conn->dataDigest == ISP_DIG_CRC32C)) {
			tmp = crc32c(pending->pdu.data, pending->pdu.dataSegmentLength);			
			crc = readOP->dataCRC;
		
			if (tmp != crc) { /* CRC don't match */
				DEBUG2("recivePDU: Data CRC Don't match! exp=0x%08x calc=0x%08x\n", crc, tmp);
				/* FIXME! reject this PDU */
				/*rejectPDU(conn, pending);*/
				cleanupPendingTask(conn);
				return RWPDU_NOTALL;
			}
		}

		if ((tsk->immediate == 0)&&(req->cmd != BHS_OPCODE_SCSI_DATA_OUT )) {
			/* checking command sequencing */
			res = counterAddValue(&ses->cmdSN, req->cmdSN);
			if (res == COUNTER_ERROR) {
				DEBUG2("recivePDU: session cmdSN:%08x while request cmdSN:%08x\n",
					ses->cmdSN.count, req->cmdSN);
				cleanupPendingTask(conn);
				return RWPDU_NOTALL;
			} else if (res == COUNTER_OUT_OF_ORDER) {
				DEBUG("recivePDU: cmdSN out of order.\n");
			} else if (res == COUNTER_DUP) {
				DEBUG("recivePDU: duplicating cmdSN.\n");
			}
		}

		/* removing acknowledged sended PDU's */
//		cleanAckStat(ses, req->expStatSN);


		/***************************************************/
#ifdef DUMP_PDU
		dumpPDU(&pending->pdu);
#endif
		/***************************************************/
/**
		if ((req->cmd & BHS_OPCODE_MASK) == 0) {
			req = req;
		}
*/

		if ((req->flags & BHS_FLAG_FINAL) == BHS_FLAG_FINAL) {/* final PDU in this task */
			if (tsk->nPDU > 1) {
				removeNode((struct DCirList *)tsk, (struct DCirList **)&ses->notAllRecived);
			}
			

			res = translateTf(ses, tsk);
			if (res == -1) {
				cleanupPendingTask(conn);
				return RWPDU_NOTALL;
			}

#if 0			
			if (tsk->immediate) {
				addNode((struct DCirList *)tsk, (struct DCirList **)&ses->immediateTasks);
			} else {
				addNode((struct DCirList *)tsk, (struct DCirList **)&ses->tasks);
			}
			/**********************************************************/
			sesTaskOp(ses);
			/**********************************************************/
#endif

		} else {
			if (tsk->nPDU == 1) {
				addNode((struct DCirList *)tsk, (struct DCirList **)&ses->notAllRecived);
			}
			return RWPDU_NOTALL;
		}
		

		return RWPDU_OK;
	}
	
	
	return RWPDU_OK;
}


static void cleanTask(struct Session *ses, struct Task *tsk)
{
	struct tfCommand *cmd = &tsk->cmd;
	
/*	freeBuff(cmd->senseBuffer);*/
	if (cmd->readBuffer != NULL) {
		freeBuff(cmd->readBuffer);
	}
	if (tsk->pdus != NULL) {
		freePDUs(ses, &tsk->pdus);
	}
	freeTask(ses, tsk);
}

int translateTfCommand(struct Session *ses, struct Task *tsk)
{
	struct tfCommand *cmd = &tsk->cmd;
	struct bhsRequest *req = (struct bhsRequest *)&tsk->pdus->pdu.bhs, *tmp;
	int res;
	struct PDUList *pdu, *pduLast;
	uint32_t prevOffset, currSize;
	
	cmd->cdbBytes = req->cdbByte;
	cmd->cdbLen = 16;
	
/*  We need task information in tfAPI ? */
/*	cmd->priv = tsk; */
	
	cmd->writeBuffs = NULL;
	cmd->imWriteBuffer = NULL;
	
	cmd->lun = req->lunL;
	cmd->senseLen = MAX_SENSE_LEN;

	/*******************/
	cmd->cmd = TF_CMD;
	cmd->stage = 0;

	cmd->lun = req->lunL;
	
	if ((req->flags & BHS_SCSI_W_FLAG) != BHS_SCSI_W_FLAG) {
		
		cmd->writeCount = 0;
		
		if ((req->flags & BHS_SCSI_R_FLAG) == BHS_SCSI_R_FLAG) {
			/* unidirectional read operation */
			cmd->residualReadCount = req->expDataTL;
		} else {
			/* no I/O */
			cmd->residualReadCount = 0;
		}
	} else {
		//dumpPDU(&tsk->pdus->pdu);
		
		cmd->writeCount = req->expDataTL;
		
		if ((req->flags & BHS_SCSI_R_FLAG) != BHS_SCSI_R_FLAG)  {
			/* unidirectional write operation */
			cmd->residualReadCount = 0;	
		} else {
			/* bidirectional operations */
			cmd->residualReadCount = tsk->pdus->pdu.ahs.ahsData[0];
		}
		
		/* Currently supported only PDUInOrder and DataSequenceInOrder */
		/* Checking data follow */
		if (tsk->nPDU > 1) {
			if (tsk->r2tRes != NULL) {
				DEBUG("translateTfCommand: Can't check R2T yet\n");
			} else {
				pduLast = pdu = tsk->pdus;
				prevOffset = 0;
				currSize = pdu->pdu.dataSegmentLength;
				
				pdu = (struct PDUList *)pdu->list.next;

				do {
					tmp = (struct bhsRequest *)&pdu->pdu.bhs;
					if ((tmp->cmd & BHS_OPCODE_MASK) == BHS_OPCODE_SCSI_DATA_OUT) {
						if (tmp->buffOffset != currSize) {
							DEBUG("translateTfCommand:	data not in oreder!\n");
						}
						prevOffset = tmp->buffOffset;
						currSize += pdu->pdu.dataSegmentLength;
					} else {
						DEBUG("translateTfCommand: Hmm unexpected PDU type!\n");
						
					}
					pdu = (struct PDUList *)pdu->list.next;
				} while (pdu != pduLast);

				if (currSize == req->expDataTL) {
					cmd->cmd |= TF_FDATA;
				} else {
					cmd->cmd |= TF_PDATA;
				}

				cmd->imWriteBuffer = tsk->dataSegment;
				cmd->imWriteBuffer->length = currSize;
			}
		} else { /* only one PDU */
			/* immediate data? */
			if (tsk->dataSegment != NULL) {
				cmd->imWriteBuffer = tsk->dataSegment;
				
				if (tsk->pdus->pdu.dataSegmentLength == cmd->writeCount) {
					cmd->cmd |= TF_FDATA; /* fulldata transfered */
				} else {
					cmd->cmd |= TF_PDATA; /* need more data */
				}
			} else {
				DEBUG1("translateTfCommand: No immediate for %08x of data\n", cmd->writeCount);
			}
			
		}

		if ((cmd->cmd & TF_PDATA) == TF_PDATA) {
			DEBUG2("translateTfCommand: Only part %08x of data %08x obtained\n",
				tsk->pdus->pdu.dataSegmentLength, cmd->writeCount);
		}

	}

	cmd->ses = ses;

	if (cmd->residualReadCount > 0) {
		cmd->readBuffer = allocBuff(ses->buffAlloc, cmd->residualReadCount);
		if (cmd->readBuffer == NULL) {
			DEBUG("translateTfCommand: Can't alooc read buffer!\n");
			return -1;
		}
	} else {
		cmd->readBuffer = NULL;
	}
	
	/*************************/
	/* What to do with cmd?? */
	/*************************************************/
	/* At these time we don't neede in a request PDU */
	/*freePDUs(ses, &tsk->pdus);*/

/*	res = ses->cclass.tfSCSICommand(ses->tfHandle, cmd); */
	cmd->classHandle = ses->cclass.handle;
	cmd->targetHandle = ses->tfHandle;

	res = ses->cclass.tfSCSICommand(cmd); 

	freePDUs(ses, &tsk->pdus);

	if (res == -1) { /* HARD ERROR */
		cleanTask(ses, tsk);
	} else {

	}
	return 0;
}


/* FIXME function not checked!!!*/
int translateTfDataOut(struct Session *ses, struct Task *tsk, struct ResponseTask *rtsk)
{
	/*struct bhsRequest *req = (struct bhsRequest *)&tsk->pdus->pdu.bhs;*/
	struct R2T *r2t;
	
	if (rtsk != NULL) {
		
		if (rtsk->origTask->r2tRes == NULL) {
			/* Unsolicated DATA */

			r2t = rtsk->origTask->r2tRes = &rtsk->r2t;
		} else {
			r2t = (struct R2T *)addNode((struct DCirList *)&rtsk->r2t, (struct DCirList **)&rtsk->origTask->r2tRes);
		}
		r2t->wBuffer = tsk->dataSegment;
		
		tsk->cmd.cmd |= TF_FDATA;
	} else {
		DEBUG("translateTf: SCSI_DATA_OUT without rtsk!\n");
	}
	
	return 0;
}


static int doNopOut(struct Session *ses, struct Task *tsk)
{
	struct bhsRequest *req = (struct bhsRequest *)&tsk->pdus->pdu.bhs;
	struct ResponseTask *rtsk;
	struct bhsResponse *res;
	uint32_t dataLen;
	
	if (req->targetTransferTag != 0xffffffff) {
		DEBUG("doNopOut: invalid TTT!\n");
		return -1;
	}

	rtsk = allocResponseTask(ses);
	if (rtsk == NULL) {
		DEBUG("doNopOut: Can't alloc response task\n");
		return -1;
	}
	
	tsk->cmd.readBuffer = NULL;
	tsk->intialRes = rtsk;
	rtsk->pdus = allocPDU(ses);	
	rtsk->origTask = tsk;
	rtsk->targetTransferTag = 0xffffffff;
	rtsk->refITT = tsk->initiatorTaskTag;
	rtsk->lun = tsk->lun;
	rtsk->iFlags = 0;
	
		
	if (rtsk->pdus == NULL) {
		DEBUG("logout: Can't alloc PDU\n");
		return -1;
	}

	if (tsk->pdus->pdu.dataSegmentLength > 0) {
		dataLen = MIN(ses->minMaxRcvDataSegment, tsk->pdus->pdu.dataSegmentLength);
		rtsk->dataSegment = allocBuff(ses->buffAlloc, dataLen); 
			
		if (rtsk->dataSegment == NULL) {
			DEBUG("doNopOut: can't alloc buffer!\n");
			return -1;
		}

		rtsk->pdus->pdu.dataSegmentLength = dataLen;
		memcpy(rtsk->dataSegment->data, tsk->dataSegment->data, dataLen);

	} else {
		rtsk->pdus->pdu.dataSegmentLength = 0;
	}
	
	res = ((struct bhsResponse *)&rtsk->pdus->pdu.bhs);
	res->cmd = BHS_OPCODE_NOPIN;
	res->totalAHSLength = 0;
	res->initiatorTaskTag = tsk->initiatorTaskTag;
	res->targetTransferTag = 0xffffffff;
	res->flags = BHS_FLAG_FINAL;
	res->response = 0;
	res->lunL = 0;

	addNode((struct DCirList *)rtsk, (struct DCirList **)&ses->resTasks);
	ses->connections->sockNeedToWrite = 1;
	return 0;
}

static int translateTf(struct Session *ses, struct Task *tsk)
{
	struct bhsRequest *req = (struct bhsRequest *)&tsk->pdus->pdu.bhs;
	switch (req->cmd & BHS_OPCODE_MASK) {
	case BHS_OPCODE_NOPOUT:
		return doNopOut(ses, tsk);
	case BHS_OPCODE_SCSI_COMMAND:
		return translateTfCommand(ses, tsk);
	case BHS_OPCODE_SCSI_DATA_OUT:
		return translateTfDataOut(ses, tsk, findResponseTask(ses, req->targetTransferTag));
	case BHS_OPCODE_LOGOUT_REQUEST:
		return logout(ses, tsk);
	default:
		DEBUG1("translateTf: UNKNOWN pdu type: %x!", req->cmd);
		return -1;
	}
	return 0;
}

static int logout(struct Session *ses, struct Task *tsk)
{
	struct ResponseTask *rtsk;
	struct bhsResponse *res;

	rtsk = allocResponseTask(ses);
	if (rtsk == NULL) {
		/* can't alloc response task*/
		DEBUG("logout: Can't alloc response task\n");
		return -1;
	}
	
	tsk->intialRes = rtsk;
	rtsk->pdus = allocPDU(ses);	
	rtsk->origTask = tsk;
	rtsk->targetTransferTag = 0xffffffff;
	rtsk->refITT = tsk->initiatorTaskTag;
	rtsk->lun = tsk->lun;
	rtsk->iFlags = 0;
		
	if (rtsk->pdus == NULL) {
		DEBUG("logout: Can't alloc PDU\n");
		return -1;
	}

	rtsk->pdus->pdu.dataSegmentLength = 0;
	
	res = ((struct bhsResponse *)&rtsk->pdus->pdu.bhs);
	res->cmd = BHS_OPCODE_LOGOUT_RESPONSE;
	res->totalAHSLength = 0;
	res->initiatorTaskTag = tsk->initiatorTaskTag;
	res->flags = BHS_FLAG_FINAL;
	res->response = 0;
	res->t.time2Retain = 0; /*****/
	res->t.time2Wait = 0;   /*****/
	res->lunL = 0;
	//res->snackTag = 0;	
	addNode((struct DCirList *)rtsk, (struct DCirList **)&ses->resTasks);
	ses->connections->sockNeedToWrite = 1;
	return 0;
}


int sesTaskOp(struct Session *ses)
{
	struct Task *tsk;
	/* check immediate tasks */
	if (ses->immediateTasks != NULL) {
		tsk = (struct Task *)allocNode((struct DCirList **)&ses->immediateTasks);
		addNode((struct DCirList *)tsk, (struct DCirList **)&ses->perfImTasks);
	} else {
		tsk = (struct Task *)allocNode((struct DCirList **)&ses->tasks);
		addNode((struct DCirList *)tsk, (struct DCirList **)&ses->perfTasks);
	}
	
	switch (tsk->pdus->pdu.bhs.cmd & BHS_OPCODE_MASK) {
	case BHS_OPCODE_SCSI_COMMAND:
		return translateTf(ses, tsk);
	case BHS_OPCODE_LOGOUT_REQUEST:
		return logout(ses, tsk);
	case 0:
		DEBUG("NOP-out pdu type!\n");
	default:
		DEBUG("sesTaskOp: UNKNOWN pdu type!");
	}

	return 0;
}



int sesQueueTask(struct Session *ses, struct Task *tsk)
{
	int res;
	/* check immediate tasks */
	//if (ses->immediateTasks != NULL) {
	if (tsk->immediate) {
		tsk = (struct Task *)removeNode((struct DCirList *)tsk, (struct DCirList **)&ses->immediateTasks);		
	} else {
		tsk = (struct Task *)removeNode((struct DCirList *)tsk, (struct DCirList **)&ses->tasks);
	}
	
//	switch (tsk->pdus->pdu.bhs.cmd & BHS_OPCODE_MASK) {
//	case BHS_OPCODE_SCSI_COMMAND:
	if ((tsk->cmd.cmd & TF_CMD) == TF_CMD) {	
		tsk->cmd.stage++;
		res = ses->cclass.tfSCSICommand(/*ses->tfHandle, */&tsk->cmd);
		if (res < 0) {
			DEBUG1("sesQueueTask: tfSCSICommand returned %d\n", res);
		}
	} else {
		DEBUG("sesTaskOp: UNKNOWN pdu type!");
	}

/*	res = ses->cclass.tfSCSICommand(NULL, cmd);*/
	return 0;
}



#if 0
/* FIXME Error! must be conn*/
int cleanAckStat(struct Session *ses, uint32_t expStatSN)
{
	struct ResponseTask *snd = ses->sended;
	struct ResponseTask *tmp;

	struct PDUList *pdu;
	struct PDUList *tmppdu;

	if (snd == NULL) {
		return 0;
	}

	do {
		pdu = snd->pdus;
		if (snd->r2t.list.next == NULL) {
			if (sIsSmaller(snd->statSN, expStatSN)) {
				
				do {
					tmppdu = (struct PDUList *)pdu->list.next;
					moveNode((struct DCirList *)pdu, (struct DCirList **)&snd->pdus, 
						(struct DCirList **)&ses->freePDUs);
					
					pdu = tmppdu;
				} while ((pdu != snd->pdus) && (snd->pdus != NULL));
				
				tmp = (struct ResponseTask *)snd->list.next;
				
				moveNode((struct DCirList *)tmp, (struct DCirList **)&ses->sended, 
					(struct DCirList **)&ses->freeTasks);
				
				snd = tmp;					
			}
		}
		
	} while ((ses->sended != snd) && (ses->sended != NULL));
	
	return 0;
}
#endif


int sesSCSIWriteReq(struct tfCommand *cmd, uint32_t buffOffset, uint32_t buffLen);
int sesSCSIManResponse(struct Session *ses, int response, void *priv);
int sesSCSICmdResponse(struct tfCommand *cmd);


static uint32_t allocTargetTaskTag(struct Session *ses)
{
	uint32_t val = ses->targetTaskTag++;
	if (val == 0xffffffff) {
		val = ses->targetTaskTag++;
	}
	return val;
}


int sesSCSIQueue(struct tfCommand *cmd)
{
	struct Session *ses = cmd->ses;	
	struct Task *tsk = tfCmd2Task(cmd);
	
	if (tsk->immediate) {
		addNode((struct DCirList *)tsk, (struct DCirList **)&ses->immediateTasks);
	} else {
		addNode((struct DCirList *)tsk, (struct DCirList **)&ses->tasks);
	}
	
	return 0;
}

#define SCSI_READ_LATER_WRITE	1
#define SCSI_CMD_RESPONSE		0

static int sesSCSIResponse(struct tfCommand *cmd, int how);

int sesSCSIReadLaterWrite(struct tfCommand *cmd)
{
	return sesSCSIResponse(cmd, SCSI_READ_LATER_WRITE);
}

int sesSCSICmdResponse(struct tfCommand *cmd)
{
	return sesSCSIResponse(cmd, SCSI_CMD_RESPONSE);
}

int sesSCSIResponse(struct tfCommand *cmd, int how)
{
	struct Session *ses = cmd->ses;	

	struct ResponseTask *rtsk;
	struct Task *tsk = tfCmd2Task(cmd);
	struct PDUList *pdu;
	struct bhsResponse *res = NULL;

	unsigned fragSize;
	//unsigned dfSize;
	unsigned pdus;
	unsigned i;
	uint32_t dataSN = 0;

	rtsk = tsk->intialRes;

	if (rtsk == NULL) {
		rtsk = allocResponseTask(ses);
		if (rtsk == NULL) {
			/* can't alloc response task*/
			DEBUG("sesSCSICmdResponse: Can't alloc response task\n");
			return -1;
		}

		tsk->intialRes = rtsk;
		rtsk->pdus = NULL;	
		rtsk->origTask = tsk;
		rtsk->targetTransferTag = allocTargetTaskTag(ses);
		rtsk->refITT = tsk->initiatorTaskTag;
		rtsk->lun = tsk->lun;
		rtsk->iFlags = 0;
	}

	/**********************************/
	if (cmd->writeCount > 0) {
		freeBuff(cmd->imWriteBuffer);
	}


	if ((cmd->residualReadCount > 0) && (rtsk->iFlags == 0) &&
		(cmd->readedCount > 0)) {
		rtsk->iFlags = 1;

		/* fragmenting Data to PDU's */
		fragSize = ses->minMaxRcvDataSegment;

		pdus = (cmd->readedCount % fragSize) ? 1 : 0;
		pdus += cmd->readedCount / fragSize;
		
		/* FIXME Acknowledge requesting */
		for (i = 0; i < pdus; i++) {
			pdu = allocPDU(ses);
			
			if (pdu == NULL) {
				DEBUG("sesSCSICmdResponse: fragmentDataBuffer: Can't alloc PDU\n");
				return -1;
			}
			
			res = ((struct bhsResponse *)&pdu->pdu.bhs);
			res->cmd = 0x25;
			res->totalAHSLength = 0;

			res->lunL = 0;
			res->snackTag = 0;
			
			/* check underflow overflow */
			if (cmd->readUnOv == RESIDUAL_UNDERFLOW) {
				res->flags = BHS_SCSI_U_FLAG;
				res->resCount = cmd->residualReadCount - cmd->readedCount;
			} else if (rtsk->origTask->cmd.readUnOv == RESIDUAL_OVERFLOW) {
				res->flags = BHS_SCSI_O_FLAG;
				res->resCount = cmd->readedCount - cmd->residualReadCount;
			} else {
				res->resCount = 0;
				res->flags = 0;
			}
			
			res->initiatorTaskTag = rtsk->origTask->initiatorTaskTag;
			
			ses->unAckSize += fragSize;
			/*res->dataSN = dataSN++;*/
			res->expDataSN = dataSN++;

			/*
			Acknowledging
			res->targetTransferTag = rtsk->targetTransferTag;
			res->lunL = rtsk->lun;
			*/

			/* * */ 
			//res->status = 0xff;


			res->buffOffset = i * fragSize;
			pdu->pdu.data = res->buffOffset + cmd->readBuffer->data;

			if (res->buffOffset + fragSize > cmd->readedCount) {
				pdu->pdu.dataSegmentLength = cmd->readedCount;
			} else {
				pdu->pdu.dataSegmentLength = fragSize;
			}
			
			addNode((struct DCirList *)pdu, (struct DCirList **)&rtsk->pdus);
		}
		
		res->flags |= BHS_FLAG_FINAL;
		

		/* FIXME! Cheking for good status, RFC, [SAM2] */
		if ((cmd->writeCount == 0) && (cmd->response == 0) && (cmd->status == 0) && (cmd->senseLen == 0)) {
			/* unidirectional read operation */
			res->flags |= BHS_SCSI_S_FLAG;
			res->status = cmd->status;
			res->resCount = 0;// cmd->readedCount;

			rtsk->iFlags |= RT_FLAG_TASK_FREE;

			addNode((struct DCirList *)rtsk, (struct DCirList **)&ses->resTasks);
			ses->connections->sockNeedToWrite = 1;
			return 0;
		}
	} /*else {
		res->expDataSN = 0;
	}*/

	if (how == SCSI_READ_LATER_WRITE) {
		/* send only readed data */
		return 0;
	}


	/* Creating SCSI-Response PDU */
	pdu = allocPDU(ses);	
	if (pdu == NULL) {
		DEBUG("sesSCSICmdResponse: Can't alloc PDU\n");
		return -1;
	}

	res = ((struct bhsResponse *)&pdu->pdu.bhs);

	res->cmd = 0x21;
	res->flags = BHS_FLAG_FINAL;
	res->response = cmd->response;
	res->status = cmd->status;
	res->initiatorTaskTag = rtsk->refITT;
	res->totalAHSLength = 0;

	res->lunL = 0;
	res->snackTag = 0;
	
	/* FIXME! More careful calculation */
	res->expDataSN = dataSN;

	/* check for write underflow overflow */
	if (cmd->writeUnOv == RESIDUAL_UNDERFLOW) {
		res->flags |= BHS_SCSI_U_FLAG;
		res->resCount = cmd->writeCount - cmd->writedCount;
	} else if (cmd->readUnOv == RESIDUAL_OVERFLOW) {
		res->flags |= BHS_SCSI_O_FLAG;
		res->resCount = cmd->writedCount - cmd->writeCount;
	} else {
		res->resCount = 0;		
	}

	/* check for read underflow overflow */
	if (cmd->readUnOv == RESIDUAL_UNDERFLOW) {
		res->flags |= BHS_SCSI_BU_FLAG;
		res->biReadResCount = cmd->residualReadCount - cmd->readedCount;
	} else if (cmd->readUnOv == RESIDUAL_OVERFLOW) {
		res->flags |= BHS_SCSI_BO_FLAG;
		res->biReadResCount = cmd->readedCount - cmd->residualReadCount;
	} else {
		res->biReadResCount = 0;		
	}
	
	/* (cmd->residualReadCount > 0) && (cmd->writeCount > 0) */
	if ((cmd->writeCount > 0) && (res->response == 0)) {
		dataSN += tsk->nR2t;
	}
	
	res->expDataSN = dataSN;

	addNode((struct DCirList *)pdu, (struct DCirList **)&rtsk->pdus);

/*	pdu->pdu.data = cmd->senseBuffer->data;*/
	if (cmd->senseLen > 0) {
		pdu->pdu.dataSegmentLength = cmd->senseLen + 2;
		cmd->senseLen = htons(cmd->senseLen);
		pdu->pdu.data = (uint8_t *)&cmd->senseLen;		
	} else {
		pdu->pdu.dataSegmentLength = 0;
	}

	rtsk->iFlags |= RT_FLAG_TASK_FREE;

	/*** FIXME: Bogus pdu setting ***/
	/* what to do with rtsk?*/
	addNode((struct DCirList *)rtsk, (struct DCirList **)&ses->resTasks);

	/************************************/
	ses->connections->sockNeedToWrite = 1;
	return 0;
}


static int fillPDU(struct InetConnection *conn, struct PDUList *pdu, int fillStatSN)
{
	struct bhsResponse *res = (struct bhsResponse *)&pdu->pdu.bhs;
	
	res->expCmdSN = counterGetValue(&conn->member->cmdSN) + 1;
	if (fillStatSN == 1) {
		res->statSN = conn->statSN++;
	} else {
		res->statSN = 0;//conn->statSN;
	}
	
	/* FIXME! */
	res->maxCmdSN = min3(conn->member->nFreePDUs, conn->member->nFreeResTasks, conn->member->nFreeTasks) + 
		res->expCmdSN;

	return 0;
}

static int cleanResTask(struct InetConnection *conn, struct ResponseTask *rtsk)
{
	/** AllPdus **/
	if (conn->member->errorRecoveryLevel == 0) {
		/*while (rtsk->pdus) {
		*	pdu = (struct PDUList *)removeNode((struct DCirList *)rtsk->pdus, (struct DCirList **)&rtsk->pdus);
		*	freePDU(conn->member, pdu);
		*}
		*/
		freePDUs(conn->member, &rtsk->pdus);

		/*
		if (rtsk->) {
			freeBuff(rtsk->dataSegment);
		}*/
	
		if (rtsk->origTask != NULL) {
			cleanTask(conn->member, rtsk->origTask);
		}
		
		freeResponseTask(conn->member, rtsk);
	} else {
		/**** FIXME!!! ****/
	}

	return 0;
}

int sendPDU(struct InetConnection *conn)
{
	struct ReadWriteOP *writeOP = &conn->writeOP;
	struct PDUList *pending = conn->wpending;
	struct Task *tsk;
	struct ResponseTask *rtsk = conn->sending;
	struct Session *ses = conn->member;

	struct bhsResponse *rsp;

	int res;
	uint32_t tmp;

	/*
	currently AHS length must be 0
		
		  errorRecoveryLevel
	*/

	if ((writeOP->stage % 2) == 1) {/* phase not ended */
		if (writeOP->stage == 5) {
			tsk = conn->pndTask;
		}
		res = sendOP(conn->scli, writeOP);
		if (res < 0) { /* error or connection error */
			DEBUG("sendPDU: writing error\n");
			return res;
		} else if (res == RWOP_TRANS_NOT_ALL) {
			
			return RWPDU_NOTALL;
		}
	}

	switch (writeOP->stage) {
	case 0: 
		/* allocating expCmdSN */

		if (conn->sending == NULL) {
			rtsk = conn->sending = (struct ResponseTask *)removeNode((struct DCirList *)ses->resTasks, 
				(struct DCirList **)&ses->resTasks);
			
			if (rtsk == NULL) {
				/*Hmm it's error?*/
				conn->sockNeedToWrite = 0;
				return RWPDU_NOTHING;
			}
		
			pending = conn->wpending = rtsk->pdus;
		} else {
			rtsk = conn->sending;
			pending = conn->wpending = (struct PDUList *)conn->wpending->list.next;
		}

		/**********************_** FIXME! **/
		rsp = (struct bhsResponse *)&pending->pdu.bhs;

		tmp = rsp->cmd & BHS_OPCODE_MASK;
		if (tmp == BHS_OPCODE_SCSI_DATA_IN) {
			fillPDU(conn, pending, BHS_ISSET(rsp->flags, BHS_SCSI_S_FLAG));
		} else {
			fillPDU(conn, pending, 1);
		}	

		/***************************************************/
#ifdef DUMP_PDU
		dumpPDU(&pending->pdu);
#endif
		/***************************************************/

		writeOP->stage = 1;
		writeOP->buffer = (uint8_t *)&pending->pdu.bhs;
		writeOP->expectedLen = conn->headerSize;
		writeOP->perfomedLen = 0;
			
		switch (tmp) {
		case BHS_OPCODE_LOGOUT_RESPONSE:
		case BHS_OPCODE_SCSI_RESP:
		case BHS_OPCODE_SCSI_DATA_IN:
			rsp->expDataSN = htonl(rsp->expDataSN);
			rsp->biReadResCount = htonl(rsp->biReadResCount);
			rsp->resCount = htonl(rsp->resCount);
		case BHS_OPCODE_NOPIN:
			rsp->snackTag = htonl(rsp->snackTag); /* ITT */
			break;
		default:
			/* BAD BHS opcode */
			DEBUG1("sendPDU: unexpected PDU opcode %d\n", rsp->cmd);
			
			/* FIXME! reject this PDU */
			return RWPDU_NOTALL;
		}
		
#ifdef LITTLE_ENDIAN
		tmp = htonl(rsp->lun[0]);
		rsp->lun[0] = htonl(rsp->lun[1]);
		rsp->lun[1] = tmp;
		rsp->initiatorTaskTag = htonl(rsp->initiatorTaskTag);
		rsp->statSN = htonl(rsp->statSN);
		rsp->expCmdSN = htonl(rsp->expCmdSN);
		rsp->maxCmdSN = htonl(rsp->maxCmdSN);
#endif
		UINT_TO_UINT24(pending->pdu.dataSegmentLength, rsp->dataSegmentLength);

		if (conn->headerDigest == ISP_DIG_CRC32C) {
			pending->pdu.headerDigest = crc32c((uint8_t *)&pending->pdu.bhs, sizeof(struct bhsBase));
		}

		if (pending->pdu.dataSegmentLength > 0) {
			if (conn->dataDigest == ISP_DIG_CRC32C) {
				writeOP->stage |= RWOP_CRCFLAG;
				writeOP->dataCRC = crc32c(pending->pdu.data, pending->pdu.dataSegmentLength);
			}			
			memcpy(pending->pdu.data - conn->headerSize, writeOP->buffer, conn->headerSize);
			
			writeOP->expectedLen = ((4 - (pending->pdu.dataSegmentLength % 4)) % 4)
				+ pending->pdu.dataSegmentLength + conn->headerSize;
			writeOP->buffer = pending->pdu.data - conn->headerSize;
		} 

		res = sendOP(conn->scli, writeOP);
		if (res < 0) { /* error or connection error */
			/* FIXME! */
			DEBUG("sendPDU: writing error 2\n");
			return res;
		} else if (res == RWOP_TRANS_NOT_ALL) {
			return RWPDU_NOTALL;
		}
		
		ses->pdus_sent++;
		ses->bytes_write += writeOP->expectedLen;
		case 2:
#if 0
		if (pending->pdu.dataSegmentLength > 0) {
			
			writeOP->stage = 3;
			writeOP->expectedLen = ((4 - (pending->pdu.dataSegmentLength % 4)) % 4)
				+ pending->pdu.dataSegmentLength;
			writeOP->perfomedLen = 0;
			writeOP->buffer = pending->pdu.data;

			if (conn->dataDigest == ISP_DIG_CRC32C) {
				writeOP->stage |= RWOP_CRCFLAG;
				writeOP->dataCRC = crc32c(pending->pdu.data, pending->pdu.dataSegmentLength);
			}
			res = sendOP(conn->scli, writeOP);
			if (res < 0) { /* error or connection error */
				/* FIXME! */
				DEBUG("sendPDU: writing error 3\n");
				return res;
			} else if (res == RWOP_TRANS_NOT_ALL) {
				return RWPDU_NOTALL;
			}

			ses->bytes_write += writeOP->expectedLen;
		}	
	case 4:
#endif

		/****************************/
		/* FIXME!! Perfoming logout */
		if (( (((struct bhsResponse *)&pending->pdu.bhs)->cmd & BHS_OPCODE_MASK) == BHS_OPCODE_LOGOUT_RESPONSE) /*&& 
			(rsp->response == 0)*/) {
			shutdown(conn->scli, 2);
			return RWOP_SHUTDOWN;
		}
		/****************************/

		writeOP->stage = 0;
		if ((struct PDUList *)conn->wpending->list.next == rtsk->pdus) {
			cleanResTask(conn, rtsk);
			conn->sending = NULL;

			if (ses->resTasks == NULL) {
				// nothing to send
				conn->sockNeedToWrite = 0;
				return RWPDU_OK;
			}
		} 
		

	}

	return RWPDU_OK;
}

static int reciveTimeOut(SOCKET s, void* buff, unsigned count, struct timeval *tv)
{
	int res;
	unsigned perfLen = 0;
	fd_set rop;

	FD_ZERO(&rop);
	FD_SET(s, &rop);
	
	for (;;) {
		res = select(s + 1, &rop, NULL, NULL, tv);
		if (res == 0) /* timeout */ {
			return -1;
		}

		res = recv(s, (char*)buff + perfLen, count, 0);
		if (((unsigned)res == count) || (res == -1) || (res == 0))   {
			return (perfLen + res);
		} else {
			perfLen += res;
			count -= res;
			if (count == 0) {
				return perfLen;
			}
		}
	}
}

static int sendTimeOut(SOCKET s, const void* buff, unsigned count, struct timeval *tv)
{
	int res;
	unsigned perfLen = 0;
	fd_set wop;

	FD_ZERO(&wop);
	FD_SET(s, &wop);
	
	for (;;) {
		res = select(s + 1, NULL, &wop, NULL, tv);
		if (res == 0) /* timeout */ {
			return -1;
		}

		res = send(s, (const char*)buff + perfLen, count, 0);
		if (((unsigned)res == count) || (res == -1) || (res == 0))   {
			return (perfLen + res);
		} else {
			perfLen += res;
			count -= res;
			if (count == 0) {
				return perfLen;
			}
		}
	}
}

int reciveFirstBHS(SOCKET s, struct PDU *pdu, struct timeval *tv)
{
	int res;
	struct bhsRequest *req;
	
	res = reciveTimeOut(s, &pdu->bhs, sizeof(struct bhsBase), tv);
	if (res < sizeof(struct bhsBase)) {
		return -1;
	}

	pdu->dataSegmentLength = UINT24_TO_UINT(pdu->bhs.dataSegmentLength);

	req = (struct bhsRequest *)&pdu->bhs;

	req->lun0 = htonl(req->lun0);
	req->lun1 = htonl(req->lun1);

	req->initiatorTaskTag = ntohl(req->initiatorTaskTag);
	req->targetTransferTag = ntohl(req->targetTransferTag);
	req->cmdSN = ntohl(req->cmdSN);
	req->expStatSN = ntohl(req->expStatSN);


	/***************************************************/
	/*dumpPDU(pdu);*/
	/***************************************************/

	return 0;
}

int reciveFirstData(SOCKET s, struct PDU *pdu, struct timeval *tv)
{
	int res;
	unsigned size;

	if (pdu->dataSegmentLength == 0) {
		/***************************************************/
#ifdef DUMP_LOGIN_PDUS
		dumpPDU(pdu);
#endif
		/***************************************************/
		return 0;
	}
	
	size = pdu->dataSegmentLength + ((4 - (pdu->dataSegmentLength % 4)) % 4);
	
	res = reciveTimeOut(s, pdu->dataSeg->data, size, tv);
	if ((unsigned)res != size) {
		return -1;
	}

	/***************************************************/
#ifdef DUMP_LOGIN_PDUS
	dumpPDU(pdu);
#endif
	/***************************************************/
	return (pdu->dataSeg->length = pdu->dataSegmentLength);
}


int reciveFirstPDU(SOCKET s, struct PDU *pdu, struct timeval *tv)
{
	int res = reciveFirstBHS(s, pdu, tv);
	if (res < 0) {
		return res;
	}
	return reciveFirstData(s, pdu, tv);
}

int sendFirstPDU(SOCKET s, struct PDU *pdu, struct timeval *tv)
{
	int res;
	unsigned size;
	unsigned psize;
	struct bhsResponse *rsp;

	/***************************************************/
#ifdef DUMP_LOGIN_PDUS
	dumpPDU(pdu);
#endif
	/***************************************************/


	UINT_TO_UINT24(pdu->dataSegmentLength, pdu->bhs.dataSegmentLength);

	rsp = (struct bhsResponse *)&pdu->bhs;

	rsp->initiatorTaskTag = htonl(rsp->initiatorTaskTag);

	rsp->lun0 = htonl(rsp->lun0);
	rsp->lun1 = htonl(rsp->lun1);

	rsp->expCmdSN = htonl(rsp->expCmdSN);
	rsp->maxCmdSN = htonl(rsp->maxCmdSN);
	rsp->statSN = htonl(rsp->statSN);

	rsp->dataSN = htonl(rsp->dataSN);


	res = sendTimeOut(s, &pdu->bhs, sizeof(struct bhsBase), tv);
	if (res < sizeof(struct bhsBase)) {
		return -1;
	}

	if (pdu->dataSegmentLength > 0) {
		psize = ((4 - (pdu->dataSegmentLength % 4)) % 4);
		size = pdu->dataSegmentLength + psize;
		while (psize != 0) {
			pdu->dataSeg->data[size - psize - 1] = 0; /**** FIXME */
			psize--;
		}
		
		res = sendTimeOut(s, pdu->dataSeg->data, size, tv);
		if ((unsigned)res != size) {
			return -1;
		}
	}


	return 0;
}


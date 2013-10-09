#include "../sock.h"
#include "../iscsi.h"
#include "iscsi_comments.h"
#include "iscsi_debug.h"

#ifdef _DEBUG2

FILE* debugFileOutput = NULL;

int initDebug(FILE* fl)
{
/*	fl = fopen("iscsi.log", "wr+"); */
	debugFileOutput = fl;
	return 0;
}


int dumpISID(struct ISID_TSIH *isid, FILE *dev)
{
	uint32_t tmp1, tmp2;
	switch ((isid->TA & ISID_T_MASK) >> ISID_T_SHIFT) {
/*
	 00b     OUI-Format
             A&B are a 22 bit OUI
             (the I/G & U/L bits are omitted)
             C&D 24 bit qualifier
     01b     EN - Format (IANA Enterprise Number)
             A - Reserved
             B&C EN (IANA Enterprise Number)
             D - Qualifier
     10b     "Random"
             A - Reserved
             B&C Random
             D - Qualifier
     11b     A,B,C&D Reserved
  */
	case 0:
		tmp1 = ((isid->TA & ISID_A_MASK) << 16) + isid->B;
		tmp2 = (isid->C << 16) + isid->D;
		fprintf(dev, " OUI:0x%06x qualifier:0x%06x", tmp1, tmp2);
		break;
	case 1:
		tmp1 = (isid->B << 8) + isid->C;
		fprintf(dev, " IANA_EN:0x%06x qualifier:0x%04x", tmp1, isid->D);
		break;
	case 2:
		tmp1 = (isid->B << 8) + isid->C;
		fprintf(dev, " Random:0x%06x qualifier:0x%04x", tmp1, isid->D);
		break;
	default:
		return 0;
	}

	fprintf(dev, " TSIH:0x%08x\n", isid->tsih);
	return 0;
}

int dumpData(struct PDU *pdu, FILE *dev)
{
	unsigned i, j;
	const char *ptr;

	if (pdu->dataSegmentLength > 0) {
		i = 0;
		ptr = pdu->data;
		
		do {			
			j = fprintf(dev, " Data: \"%s\"\n", ptr) - 9;
			i += j;
			ptr += j;
		} while (i < pdu->dataSegmentLength);
	}
	return 0;
}

static int dumpNOPOut(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " LUN:0x%08x%08x TTT:0x%08x \n"
				 " ITT:0x%08x CmdSN:0x%08x ExpStatSN:0x%08x\n", 
				bhs->lun[0], bhs->lun[1],
				bhs->targetTransferTag, bhs->initiatorTaskTag,
				bhs->cmdSN, bhs->expDataSN);
	return 0;
}

static int dumpLoginRequest(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " T:%d C:%d CSG:%d NSG:%d verMin:%d verMax:%d\n"
				 " ITT:0x%08x CID:0x%04x CmdSN:0x%08x\n", 
				BHS_ISSET(bhs->flags, BHS_LOGIN_T_BIT), BHS_ISSET(bhs->flags, BHS_LOGIN_C_BIT),
				(bhs->flags & BHS_LOGIN_CSG_MSK) >> 2, bhs->flags & BHS_LOGIN_NSG_MSK,
				bhs->versionMin, bhs->versionMax, bhs->initiatorTaskTag, 
				bhs->l.connectionID, bhs->cmdSN);
	dumpISID(&bhs->isid_tsih, dev);
	return 0;
}


static int dumpTextRequest(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " I:%d F:%d C:%d TTT:0x%08x \n"
				 " ITT:0x%08x CmdSN:0x%08x ExpStatSN:0x%08x\n", 
				BHS_ISSET(bhs->cmd, BHS_OPCODE_IMMEDIATE_BIT),
				BHS_ISSET(bhs->flags, BHS_FLAG_FINAL),
				BHS_ISSET(bhs->flags, BHS_FLAG_CONTINUE),
				bhs->targetTransferTag, bhs->initiatorTaskTag,
				bhs->cmdSN, bhs->expDataSN);
	return 0;
}



static int dumpSCSIRequest(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " I:%d F:%d R:%d W:%d Attr:%d ITT:0x%08x"
				 " LUN:0x%08x%08x\n (expDataTL:0x%08x)"
				 " CmdSN:0x%08x ExpStatSN:0x%08x\n"
				 " CDB:%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x\n",				 
				BHS_ISSET(bhs->cmd, BHS_OPCODE_IMMEDIATE_BIT),
				BHS_ISSET(bhs->flags, BHS_FLAG_FINAL),
				BHS_ISSET(bhs->flags, BHS_SCSI_R_FLAG),
				BHS_ISSET(bhs->flags, BHS_SCSI_W_FLAG),
				bhs->flags & BHS_SCSI_ATTR_MASK, bhs->initiatorTaskTag,
				bhs->lun[0], bhs->lun[1], bhs->expDataTL,
				bhs->cmdSN, bhs->expStatSN,
				bhs->cdbByte[0], bhs->cdbByte[1], bhs->cdbByte[2], bhs->cdbByte[3], 
				bhs->cdbByte[4], bhs->cdbByte[5], bhs->cdbByte[6], bhs->cdbByte[7], 
				bhs->cdbByte[8], bhs->cdbByte[9], bhs->cdbByte[10], bhs->cdbByte[11], 
				bhs->cdbByte[12], bhs->cdbByte[13], bhs->cdbByte[14], bhs->cdbByte[15]);
	return 0;
}


static int dumpDataOut(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " F:%d ITT:0x%08x TTT:0x%08x\n"
				 " LUN:0x%08x%08x ExpStatSN:0x%08x\n"
				 " DataSN:0x%08x BOff:0x%08x\n",
				BHS_ISSET(bhs->flags, BHS_FLAG_FINAL),
				bhs->initiatorTaskTag, bhs->targetTransferTag, 
				bhs->lun[0], bhs->lun[1], bhs->expStatSN,
				bhs->dataSN, bhs->buffOffset);
	return 0;
}

static int dumpLogoutRequest(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " I:%d ReasonC:%d ITT:0x%08x \n"
				 " CID:0x%04x CmdSN:0x%08x ExpStatSN:0x%08x\n",
				 BHS_ISSET(bhs->cmd, BHS_OPCODE_IMMEDIATE_BIT),
				 bhs->flags & BHS_LOGOUT_RCODE, bhs->initiatorTaskTag,
				 bhs->l.connectionID, bhs->cmdSN, bhs->expStatSN);
	return 0;
}


static int dumpTaskMFRequest(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " I:%d Function:0x%02x ITT:0x%08x \n"
				 " LUN:0x%08x%08x RTT:0x%08x\n"
				 " CmdSN:0x%08x ExpStatSN:0x%08x\n"
				 " RefCmdSN:0x%08x ExpDataSN:0x%08x\n",
				 BHS_ISSET(bhs->cmd, BHS_OPCODE_IMMEDIATE_BIT),
				 bhs->flags & BHS_TM_FUNCTION_MASK, bhs->initiatorTaskTag,
				 bhs->lun[0], bhs->lun[1], bhs->refTaskTag,
				 bhs->cmdSN, bhs->expStatSN,
				 bhs->refCmdSN, bhs->expDataSN);

	return 0;
}
/*
static int dumpDataOut(struct bhsRequest *bhs, FILE *dev)
{
	fprintf(dev, " F:%d LUN:0x%08x%08x ITT:0x%08x \n"
				 " TTT:0x%08x ExpStatSN:0x%08x DataSN:0x%08x BuffOffset:0x%08x\n",
				 BHS_ISSET(bhs->flags, BHS_FLAG_FINAL),
				 bhs->lun[0], bhs->lun[1], bhs->initiatorTaskTag,
				 bhs->targetTransferTag,
				 bhs->expStatSN, bhs->dataSN, bhs->buffOffset);
	return 0;
}
*/

/* RESPONSES */
int dumpSCSIResponse(struct bhsResponse *bhs, FILE *dev)
{
	fprintf(dev, " o:%d u:%d O:%d U:%d Res:0x%02x Stat:0x%02x"
				 " LUN:0x%08x%08x ITT:0x%08x \n"
				 " StatSN:0x%08x ExpCmdSN:0x%08x MaxCmdSN:0x%08x\n"
				 " ExpDataSN:0x%08x SNACK:0x%08x RC:0x%08x BRC:0x%08x\n",
				 BHS_ISSET(bhs->flags, BHS_SCSI_BO_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_BU_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_O_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_U_FLAG),
				 bhs->response,
				 bhs->status, bhs->lun[0], bhs->lun[1], bhs->initiatorTaskTag,
				 bhs->statSN, bhs->expCmdSN, bhs->maxCmdSN,
				 bhs->expDataSN, bhs->snackTag, bhs->resCount, bhs->biReadResCount);
	return 0;
}

int dumpDataIn(struct bhsResponse *bhs, FILE *dev)
{
	fprintf(dev, " F:%d A:%d O:%d U:%d S:%d Stat:0x%02x "
				 " LUN:0x%08x%08x ITT:0x%08x \n"
				 " StatSN:0x%08x ExpCmdSN:0x%08x MaxCmdSN:0x%08x\n"
				 " DataSN:0x%08x BuffOffset:0x%08x RC:0x%08x \n",			
				 BHS_ISSET(bhs->flags, BHS_FLAG_FINAL),
				 BHS_ISSET(bhs->flags, BHS_SCSI_A_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_O_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_U_FLAG),
				 BHS_ISSET(bhs->flags, BHS_SCSI_S_FLAG),
				 bhs->status, 
				 bhs->lun[0], bhs->lun[1],bhs->initiatorTaskTag,
				 bhs->statSN, bhs->expCmdSN, bhs->maxCmdSN,
				 bhs->dataSN, bhs->buffOffset, bhs->resCount);
	return 0;
}

static int dumpLoginResponse(struct bhsResponse *bhs, FILE *dev)
{
	fprintf(dev, " T:%d C:%d CSG:%d NSG:%d verAct:%d verMax:%d\n"
				 " ITT:0x%08x StatClass:0x%02x StatDet:0x%02x\n",
				 /*" StatSN:0x%08x ExpCmdSN:0x%08x MaxCmdSN:0x%08x\n",*/
				BHS_ISSET(bhs->flags, BHS_LOGIN_T_BIT), BHS_ISSET(bhs->flags, BHS_LOGIN_C_BIT),
				(bhs->flags & BHS_LOGIN_CSG_MSK) >> 2, bhs->flags & BHS_LOGIN_NSG_MSK,
				bhs->versionActive, bhs->versionMax, bhs->initiatorTaskTag, 
				bhs->s.statusClass, bhs->s.statusDetail/*,
				bhs->statSN, bhs->expCmdSN, bhs->maxCmdSN*/
				);
	fprintf(dev, " StatSN:0x%08x ExpCmdSN:0x%08x MaxCmdSN:0x%08x\n", bhs->statSN, bhs->expCmdSN, bhs->maxCmdSN);
	dumpISID(&bhs->isid_tsih, dev);
	return 0;
}


int dumpPDUDev(struct PDU *pdu, FILE *dev)
{
	int opcode = pdu->bhs.cmd & BHS_OPCODE_MASK;
	fprintf(dev, "BHS: opcode=0x%x (%s)\n", opcode, opcode_comment[opcode]);

	switch(opcode) {
	case 0x00:
		return dumpNOPOut((struct bhsRequest *)&pdu->bhs, dev);
	case 0x01:
		return dumpSCSIRequest((struct bhsRequest *)&pdu->bhs, dev);
	case 0x02:
		return dumpTaskMFRequest((struct bhsRequest *)&pdu->bhs, dev);
	case 0x03:
		dumpLoginRequest((struct bhsRequest *)&pdu->bhs, dev);
		return dumpData(pdu, dev);
	case 0x04:
		dumpTextRequest((struct bhsRequest *)&pdu->bhs, dev);
		return dumpData(pdu, dev);
	case 0x05:
		return dumpDataOut((struct bhsRequest *)&pdu->bhs, dev);
	case 0x06:
		return dumpLogoutRequest((struct bhsRequest *)&pdu->bhs, dev);

	case 0x21:
		return dumpSCSIResponse((struct bhsResponse *)&pdu->bhs, dev);
	case 0x23:
		dumpLoginResponse((struct bhsResponse *)&pdu->bhs, dev);
		return dumpData(pdu, dev);
	case 0x25:
		return dumpDataIn((struct bhsResponse *)&pdu->bhs, dev);
	default:
		 fprintf(dev, " unknown BHS format!\n");
	}
	return 0;
}


#endif /* _DEBUG */

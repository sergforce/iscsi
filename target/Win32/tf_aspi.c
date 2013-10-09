#include "../sock.h"
#include "../iscsi.h"
#include "../dcirlist.h"
#include "../iscsi_mem.h"
#include "../tf.h"
#include "../iscsi_param.h"
#include "../conf_reader.h"
#include "../param_helper.h"
#include "../iscsi_session.h"

#include <stdlib.h>

#include "../debug/iscsi_debug.h"

#include "wnaspi/srbcmn.h"
#include "wnaspi/srb32.h"
#include "wnaspi/scsidefs.h"


#include "tf_aspi.h"


#if (TF_EXTRA_LEN < 64)
#error  TF_EXTRA_LEN is too small
#endif

/**********************************************/

void *aspi_tfInit()
{
	LPASPIDATA lpaspi;
	HMODULE hDLL;
	DWORD dwGetASPI;

	lpaspi = (LPASPIDATA)malloc(sizeof(struct tagASPIDATA));
	if (lpaspi == NULL) {
		return NULL;
	}

	hDLL = lpaspi->hWnaspiDll = LoadLibrary("WNASPI32.DLL");
	
	if (lpaspi->hWnaspiDll == NULL) {
		free(lpaspi);
		return NULL;
	}

	lpaspi->lpGetASPI32SupportInfo = (DWORD (*)(void))GetProcAddress(hDLL, 
		"GetASPI32SupportInfo");
	lpaspi->lpSendASPI32Command = (DWORD (*)(LPSRB))GetProcAddress(hDLL, 
		"SendASPI32Command");
	lpaspi->lpGetASPI32Buffer = (BOOL (*)(PASPI32BUFF))GetProcAddress(hDLL, 
		"GetASPI32Buffer");
	lpaspi->lpFreeASPI32Buffer = (BOOL (*)(PASPI32BUFF))GetProcAddress(hDLL, 
		"FreeASPI32Buffer");
	lpaspi->lpTranslateASPI32Address = (BOOL (*)(PDWORD, PDWORD))GetProcAddress(hDLL, 
		"TranslateASPI32Address");		
	
	lpaspi->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	dwGetASPI = lpaspi->lpGetASPI32SupportInfo();
	if (HIBYTE(LOWORD(dwGetASPI)) == SS_COMP) {
		lpaspi->nDevice = LOBYTE(LOWORD(dwGetASPI));
	} else {
		CloseHandle(lpaspi->hEvent);
		FreeLibrary(hDLL);
		free(lpaspi);
		return NULL;
	}

	return lpaspi;
}

void aspi_tfCleanup(void *handle)
{
	LPASPIDATA lpaspi = (LPASPIDATA)handle;

	CloseHandle(lpaspi->hEvent);
	FreeLibrary(lpaspi->hWnaspiDll);
	free(lpaspi);
}


static int fillAspiTargetData(struct confElement *head, LPASPITARGETDATA lpad)
{
	struct confElement *haId, *targetId, *lun;
	int iHaId, iTargetId, iLun;

	haId = getElemInt(head, ASPIN_HAID, &iHaId);
	targetId = getElemInt(head, ASPIN_TARGETID, &iTargetId);
	lun = getElemInt(head, ASPIN_LUN, &iLun);

	if ((haId == NULL) || (targetId == NULL) || (lun == NULL)) {
		return -1;
	}

	lpad->haId = iHaId;
	lpad->lun = iLun;
	lpad->reserved = 0xff;
	lpad->targetId = iTargetId;
	return 0;
}

void *aspi_tfAttach(void *handle, struct Session *ses)
{
	SRB32_GetSetTimeouts srb;
	SRB32_BusDeviceReset reset;
	DWORD status;
	int res;

	LPASPITARGETDATA atd;
	LPASPIDATA lpaspi = (LPASPIDATA)handle;
	struct confElement *targetInfo = (struct confElement *)ses->params[ISP_TARGETNAME].pvalue;
	
	atd = (LPASPITARGETDATA)malloc(sizeof(ASPITARGETDATA));
	if (atd == NULL) {
		return NULL;
	}

	atd->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (atd->hEvent == INVALID_HANDLE_VALUE) {
		free(atd);
		return NULL;
	}
	res = fillAspiTargetData(targetInfo, atd);

	if (res == -1) {
		DEBUG1("aspi_tfAttach: Device parameters incorrect for target: %s\n", targetInfo->name);
		free(atd);
		return NULL;
	}

	/* setting timeout */
	srb.SRB_Cmd = SC_GETSET_TIMEOUTS;
	srb.SRB_Flags = SRB_DIR_OUT;
	srb.SRB_Status = 0;	
	srb.SRB_HaId = atd->haId;
	srb.SRB_Target = atd->targetId;
	srb.SRB_Lun = atd->lun;	
	srb.SRB_Timeout = 50;	
	status = lpaspi->lpSendASPI32Command(&srb);
	if (status != SS_COMP) {
		free(atd);
		return NULL;
	}

	/* resetting device */
	reset.SRB_Cmd = SC_RESET_DEV;
	reset.SRB_Flags = 0;
	reset.SRB_Status = 0;
	reset.SRB_HaId = atd->haId;
	reset.SRB_Target = atd->targetId;
	reset.SRB_Lun = atd->lun;		
	status = lpaspi->lpSendASPI32Command((LPSRB)&reset);
	
	return atd;
}

void aspi_tfDetach(void *handle, void *prot)
{
	LPASPITARGETDATA atd = (LPASPITARGETDATA)prot;
	CloseHandle(atd->hEvent);
	free(prot);
}


#define FRAG_LENGTH	65536
struct fragData {
	uint32_t num;
	uint32_t next;

	char *origPointer;
	uint32_t origLen;
	
	char *newPointer;
	uint32_t newLen;
		
	uint32_t dataToPlay;

	uint32_t lba;
	uint32_t blockSize;
	uint16_t smBlkSize;	
};


static int makeFrag(const char *origCmd, struct fragData *fd, char *newCmd)
{
	uint32_t extra;
	
	if (fd->num == 0) { /* first Call */
		switch (origCmd[0]) {
		case 0x2a: /* WRITE (10) */
		case 0x28: /* READ (10) */
			fd->lba = ntohl(*(uint32_t *)&origCmd[2]);
			fd->smBlkSize = ntohs(*(uint16_t *)&origCmd[7]);

			fd->blockSize = fd->origLen / fd->smBlkSize;
			break;
		default:
			DEBUG("makeFrag unknown command!\n");
			return -1;
		}

		*(uint16_t *)&newCmd[7] = htons((uint16_t)(FRAG_LENGTH / fd->blockSize));

		fd->newPointer = fd->origPointer;
		fd->newLen = FRAG_LENGTH;
		fd->num++;
		fd->dataToPlay = fd->origLen - FRAG_LENGTH;

		fd->lba += FRAG_LENGTH / fd->blockSize;

		return 1;
	} else {
		extra = (fd->dataToPlay > FRAG_LENGTH) ? FRAG_LENGTH : fd->dataToPlay;
		*(uint32_t *)&newCmd[2] = htonl(fd->lba);

		*(uint16_t *)&newCmd[7] = htons( (uint16_t)(extra / fd->blockSize) );

		fd->newPointer += FRAG_LENGTH;
		fd->newLen = extra;

		if (extra < FRAG_LENGTH) {
			fd->dataToPlay = 0;
		} else {
			fd->dataToPlay -= FRAG_LENGTH;
		}

		if (fd->dataToPlay == 0) {
			return 0;
		}
		fd->lba += extra;
		return 1;
	}


}

int aspi_tfSCSICommand(struct tfCommand *cmd)
{
	DWORD dwASPI;
	LPSRB32_ExecSCSICmd acmd = (LPSRB32_ExecSCSICmd)cmd->extra;

	LPASPITARGETDATA lpTargData = (LPASPITARGETDATA)cmd->targetHandle;
	LPASPIDATA lpAspi = (LPASPIDATA)cmd->classHandle;

	int res;
	struct fragData fd;
	fd.num = 0;

	/* Cyrrently supported only one LUN */
	if (cmd->lun > 0) {
		cmd->response = 1;
		cmd->readedCount = 0;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		goto apsi_exit;
	}

	memset(acmd, 0, sizeof(acmd));

	acmd->SRB_Cmd = SC_EXEC_SCSI_CMD;
	acmd->SRB_Flags = SRB_EVENT_NOTIFY;
	
	acmd->SRB_HaId = lpTargData->haId;
	acmd->SRB_Target = lpTargData->targetId;
	acmd->SRB_Lun = lpTargData->lun;

	acmd->SRB_Status = 0;
	acmd->SRB_SenseLen = SENSE_LEN;

	acmd->SRB_PostProc = (LPVOID)lpTargData->hEvent;

	if ((cmd->writeCount > 0)&&(cmd->residualReadCount > 0)) {
		DEBUG("aspi_tfSCSICommand: ASPI Driver can't operation with bidirection commands!!!\n");
		/***** FIXME CLEANUP ******/
		return -1;
	}

	if (cmd->writeCount > 0) {
		acmd->SRB_BufPointer = cmd->imWriteBuffer->data;
		acmd->SRB_BufLen = cmd->writeCount;
		acmd->SRB_Flags |= SRB_DIR_OUT;
	
	} else if (cmd->residualReadCount > 0) {
		acmd->SRB_BufPointer = cmd->readBuffer->data;
		acmd->SRB_BufLen = cmd->residualReadCount;
		acmd->SRB_Flags |= SRB_DIR_IN;
	} else {
		acmd->SRB_BufPointer = NULL;
		acmd->SRB_BufLen = 0;

		acmd->SRB_Flags |= SRB_DIR_IN;
	}
	
	memcpy(acmd->CDBByte, cmd->cdbBytes, 16);

	switch ((acmd->CDBByte[0] & 0xE0) >> 5) {	
	case 0:
		acmd->SRB_CDBLen = 6;
		break;
	case 1:
	case 2:
		acmd->SRB_CDBLen = 10;
		break;
	case 4:
		acmd->SRB_CDBLen = 12;
		break;
	case 5:
		acmd->SRB_CDBLen = 16;
		break;
	default:
		DEBUG1("aspi_tfSCSICommand UNKNOWN size for command %x\n", acmd->CDBByte[0]);
		acmd->SRB_CDBLen = 0;
	}

	/* FRAGMENTING IF NEEDED */
	if (acmd->SRB_BufLen > FRAG_LENGTH) {
		fd.origPointer = acmd->SRB_BufPointer;
		fd.origLen = acmd->SRB_BufLen;

nextSRB:
		res = makeFrag(cmd->cdbBytes, &fd, acmd->CDBByte);

		acmd->SRB_BufPointer = fd.newPointer;
		acmd->SRB_BufLen = fd.newLen;

		/*if (res == 0) {
			goto fill_sence;
		}*/
	}


	/*acmd->SRB_PostProc = (LPVOID)hEvent;*/

	if (acmd->CDBByte[0] == 0xa0) {
		cmd->response = 0;
		cmd->status = 0;
		cmd->readedCount = cmd->residualReadCount;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		memset(cmd->readBuffer->data, 0, cmd->residualReadCount);
		cmd->readBuffer->data[3] = 8;
		cmd->readBuffer->data[9] = 0x00;
		return sesSCSICmdResponse(cmd);
	}

	dwASPI = lpAspi->lpSendASPI32Command((LPSRB)acmd);
	if (dwASPI == SS_PENDING) {
		WaitForSingleObject(lpTargData->hEvent, 60000);
	} 
	/* FIXME! Error cheking for dwASPI */

	if ((fd.num > 0) && (res == 1)) {
		goto nextSRB;
	}

	if ((acmd->SRB_Status == SS_COMP) || (acmd->SRB_Status == SS_ERR) || (acmd->SRB_Status == SS_ABORTED)){
		cmd->response = 0;
		cmd->status = acmd->SRB_TargStat;

		if ((acmd->SRB_Status == SS_COMP)) {
			acmd->SRB_SenseLen = 0;
		}
		if ((cmd->status == 0) && (acmd->SRB_HaStat == 0)) {
			acmd->SRB_SenseLen = 0;
		}
		
		if ((acmd->SRB_Status == SS_ERR) && (acmd->SRB_PortStat != SS_COMP)) {
			DEBUG("aspi_tfSCSICommand: Port Failure!\n");
		}

		if (acmd->SRB_Status == SS_ABORTED)		{
			cmd->response = 1;
			cmd->readedCount = 0;
			cmd->writedCount = 0;
			cmd->senseLen = 0;
			goto apsi_exit;
		}

		cmd->senseLen = acmd->SRB_SenseLen;
		if (cmd->senseLen > 0) {
			memcpy (cmd->sense, acmd->SenseArea, cmd->senseLen);
		}

		if ((cmd->writeCount > 0)) {
			cmd->writedCount = cmd->writeCount; /*acmd->SRB_BufLen;*/
			cmd->readedCount = 0;
		} else if ((cmd->residualReadCount > 0) && (cmd->senseLen == 0) ) {
			cmd->readedCount = cmd->residualReadCount; /*acmd->SRB_BufLen;*/
			cmd->writeCount = 0;
		} else {
			cmd->writeCount = 0;
			cmd->readedCount = 0;
		}
	

	} else {
		cmd->response = 1;
		cmd->readedCount = 0;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		DEBUG2("aspi_tfSCSICommand: HaStat=%x, TargStat=%x\n", acmd->SRB_HaStat, acmd->SRB_TargStat);
	}
apsi_exit:
	return sesSCSICmdResponse(cmd);
}




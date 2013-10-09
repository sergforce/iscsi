// aspiTest.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <winsock2.h>
#include <windows.h>
#include "wnaspi32.h"

#include <stdio.h>

#pragma comment(lib, "wsock32.lib")

#pragma pack (1)
#pragma comment (lib, "wnaspi32.lib")
int showDeviceBlock(HANDLE hEventASPI, int haId, int trg);
int showReadyDevice(HANDLE hEventASPI, int haId, int trg);


int main(int argc, char* argv[])
{
	DWORD dwGetASPI = GetASPI32SupportInfo ();
	DWORD dwStatus;
	int i, j, k;

	if (HIBYTE(LOWORD(dwGetASPI)) != SS_COMP) {
		printf("Initialization error!\n");
		return -1;
	}

	BYTE bNumDevice = LOBYTE (LOWORD(dwGetASPI));
	if (bNumDevice == 0) {
		printf("No devices!\n");
		return -1;
	}
	printf("Found %d devices\n",bNumDevice); 

	SRB_HAInquiry haInq;
	SRB_GetSetTimeouts srb;
	SRB_BusDeviceReset reset;

//	HINSTANCE ldw = LoadLibrary("scsiport.sys");


	char buffer[256];
	HANDLE hEventASPI = CreateEvent(NULL, TRUE, FALSE, NULL);

	ResetEvent(hEventASPI);

	SRB_ExecSCSICmd Exec;

	k = 0;

	for (i = 0; i < bNumDevice; i++) {
		memset(&haInq, 0, sizeof(SRB_HAInquiry));
		haInq.SRB_Cmd = SC_HA_INQUIRY;
		haInq.SRB_HaId = i;

		dwStatus = SendASPI32Command((LPSRB) &haInq);
		while (!haInq.SRB_Status);

		printf("%d: SCSI_ID:%d %s %s\n", i, haInq.HA_SCSI_ID, 
				haInq.HA_ManagerId,	haInq.HA_Identifier);
		
		for (j = 0; j < 16; j++) {
			if (dwStatus == SS_COMP) {
				srb.SRB_Cmd = SC_GETSET_TIMEOUTS;
				srb.SRB_HaId = i;
				srb.SRB_Flags = SRB_DIR_OUT;
				srb.SRB_Target = j;
				srb.SRB_Lun = 0;
				srb.SRB_Status = 0;
				
				srb.SRB_Timeout = 1;
				
				SendASPI32Command(&srb);
				
				
				reset.SRB_Cmd = SC_RESET_DEV;
				reset.SRB_HaId = i;
				reset.SRB_Target = j;
				reset.SRB_Lun = 0;
				reset.SRB_Status = 0;
				dwGetASPI = SendASPI32Command((LPSRB)&reset);
				

				memset(&Exec, 0, sizeof(SRB_ExecSCSICmd));
				Exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
				Exec.SRB_Flags = SRB_DIR_IN | SRB_EVENT_NOTIFY  | SRB_ENABLE_RESIDUAL_COUNT;
				
				Exec.SRB_HaId = i;
				Exec.SRB_Target = j;
				Exec.SRB_Lun = 0;
				Exec.SRB_BufLen = 256;
				Exec.SRB_BufPointer = (BYTE*)buffer;
				Exec.SRB_SenseLen = SENSE_LEN;
				Exec.SRB_CDBLen = 6;
				//Exec.SRB_CDBLen = 10;
				Exec.SRB_PostProc = (LPVOID)hEventASPI;
				
				Exec.CDBByte[0] = 0x12;
				Exec.CDBByte[1] = 0x00;
				Exec.CDBByte[4] = 0xff;

				
				//Exec.CDBByte[0] = 0x25;
				
				
				
				dwGetASPI = SendASPI32Command((LPSRB)&Exec);
				
				if (dwGetASPI == SS_PENDING) {
					WaitForSingleObject(hEventASPI, 2000);
				}
				
				if (Exec.SRB_Status == SS_COMP) {
					
					int val = buffer[0] & 0x1f;
					
					char device[9]; device[8]=0;
					char rev[17]; rev[16]=0;
					
					strncpy(device, &buffer[8], 8);
					strncpy(rev, &buffer[16], 16);
					
					printf("\t%d (%d:%d): %s %s (0x%x)\n",k, i, j, device, rev, val);
					k++;
					
					int ready = showReadyDevice(hEventASPI, i, j);
					if (ready != -1) {
						showDeviceBlock(hEventASPI, i, j);
					}
				} 
			}
		}


	}

	printf ("\nFounded %d devices\n", k);
	
	getchar();	

	return 0;
}

int showDeviceBlock(HANDLE hEventASPI, int haId, int trg)
{
	SRB_ExecSCSICmd Exec;
	char buffer[256];

	ResetEvent(hEventASPI);

	memset(&Exec, 0, sizeof(SRB_ExecSCSICmd));
	Exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
	Exec.SRB_Flags = SRB_DIR_IN | SRB_EVENT_NOTIFY  | SRB_ENABLE_RESIDUAL_COUNT;
				
	Exec.SRB_HaId = haId;
	Exec.SRB_Target = trg;
	Exec.SRB_Lun = 0;
	Exec.SRB_BufLen = 8;
	Exec.SRB_BufPointer = (BYTE*)buffer;
	Exec.SRB_SenseLen = SENSE_LEN;
	Exec.SRB_CDBLen = 10;
	Exec.SRB_PostProc = (LPVOID)hEventASPI;
			
	Exec.CDBByte[0] = 0x25;
	Exec.CDBByte[1] = 0x00;

	DWORD dwGetASPI = SendASPI32Command((LPSRB)&Exec);
			
	if (dwGetASPI == SS_PENDING) {
		WaitForSingleObject(hEventASPI, 2000);
	}
				
	if (Exec.SRB_Status == SS_COMP) {
		int blocks = ntohl(*(int*)buffer);
		int bl2 = ntohl(*(int*)(buffer + 4));
		printf("\t\tCluster: %d, Total: %d (%.2f MB)\n", bl2, blocks, (float)(((float)blocks * bl2) / 1024.0 / 1024.0));

	}
	return 0;
}



int showReadyDevice(HANDLE hEventASPI, int haId, int trg)
{
	SRB_ExecSCSICmd Exec;
	char buffer[256];

	ResetEvent(hEventASPI);

	memset(&Exec, 0, sizeof(SRB_ExecSCSICmd));
	Exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
	Exec.SRB_Flags = SRB_DIR_IN | SRB_EVENT_NOTIFY  | SRB_ENABLE_RESIDUAL_COUNT;
				
	Exec.SRB_HaId = haId;
	Exec.SRB_Target = trg;
	Exec.SRB_Lun = 0;
	Exec.SRB_BufLen = 8;
	Exec.SRB_BufPointer = (BYTE*)buffer;
	Exec.SRB_SenseLen = SENSE_LEN;
	Exec.SRB_CDBLen = 6;
	Exec.SRB_PostProc = (LPVOID)hEventASPI;
			
	DWORD dwGetASPI = SendASPI32Command((LPSRB)&Exec);
			
	if (dwGetASPI == SS_PENDING) {
		WaitForSingleObject(hEventASPI, 2000);
	}
				
	if (Exec.SRB_Status != SS_COMP) {
		printf("\t\tNot ready!\n");
		return -1;
	}
	return 0;
}
// scsiMiniport.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <devioctl.h>
//#include <ntdddisk.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>


typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH spt;
    ULONG             Filler;      // realign buffers to double word boundary
    UCHAR             ucSenseBuf[32];
    UCHAR             ucDataBuf[512];
    } SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;

typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG             Filler;      // realign buffer to double word boundary
    UCHAR             ucSenseBuf[32];
    } SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

VOID
PrintInquiryData(PCHAR  DataBuffer);
PUCHAR
AllocateAlignedBuffer(ULONG size, ULONG Align);
VOID
PrintStatusResults(
    BOOL status,DWORD returned,PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb,
    ULONG length);
VOID
PrintSenseInfo(PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb);
VOID
PrintError(ULONG ErrorCode);

int main(int argc, char* argv[])
{
	CHAR buffer[2048];
    //CHAR device[] = "\\\\.\\PhysicalDrive0";
	CHAR device[] = "\\\\.\\E:";
	//"\\\\.\\Scsi2:";
    BOOL status = 0;
    DWORD accessMode = 0, shareMode = 0;
    HANDLE fileHandle = NULL;
    ULONG length = 0, errorCode = 0, returned = 0;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;
    PUCHAR dataBuffer = NULL;
	IO_SCSI_CAPABILITIES capabilities;
	UINT sectorSize = 2048;
	
	shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;  // default
    accessMode = GENERIC_WRITE | GENERIC_READ;       // default

    fileHandle = CreateFile(device,
       accessMode,
       shareMode,
       NULL,
       OPEN_EXISTING,
       0,
       NULL);

    if (fileHandle == INVALID_HANDLE_VALUE) {
       printf("Error opening %s. Error: %d\n",
          device, errorCode = GetLastError());
       return -1;
    }

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_GET_INQUIRY_DATA,
                             NULL,
                             0,
                             buffer,
                             sizeof(buffer),
                             &returned,
                             FALSE);

    if (!status) {
       printf( "Error reading inquiry data information; error was %d\n",
          errorCode = GetLastError());
       return -1;
    }

    printf("Read %Xh bytes of inquiry data Information.\n\n",returned);

    PrintInquiryData(buffer);

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_GET_CAPABILITIES,
                             NULL,
                             0,
                             &capabilities,
                             sizeof(IO_SCSI_CAPABILITIES),
                             &returned,
                             FALSE);

    if (!status ) {
       printf( "Error in io control; error was %d\n",
          errorCode = GetLastError() );
       return -1;
    }




    ZeroMemory(&sptdwb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
    dataBuffer = AllocateAlignedBuffer(sectorSize,capabilities.AlignmentMask);
    FillMemory(dataBuffer,sectorSize/2,'N');
    FillMemory(dataBuffer + sectorSize/2,sectorSize/2,'T');

	//sptdwb.sptd.
    sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptdwb.sptd.PathId = 0;
    sptdwb.sptd.TargetId = 0;
    sptdwb.sptd.Lun = 0;
    sptdwb.sptd.CdbLength = 10;
    sptdwb.sptd.SenseInfoLength = 24;
    sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    sptdwb.sptd.DataTransferLength = 8;//0x36;//sectorSize;
    sptdwb.sptd.TimeOutValue = 2;
    sptdwb.sptd.DataBuffer = dataBuffer;
    sptdwb.sptd.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,ucSenseBuf);
    sptdwb.sptd.Cdb[0] = 0x46;
    //sptdwb.sptd.Cdb[4] = 0x30;                         // Data mode
    length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_PASS_THROUGH_DIRECT,
                             &sptdwb,
                             length,
                             &sptdwb,
                             length,
                             &returned,
                             FALSE);
  
	PrintStatusResults(status,returned,
       (PSCSI_PASS_THROUGH_WITH_BUFFERS)&sptdwb,length);

	getchar();
	return 0;
}



VOID
PrintError(ULONG ErrorCode)
{
    CHAR errorBuffer[80];
    ULONG count;

    count = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  ErrorCode,
                  0,
                  errorBuffer,
                  sizeof(errorBuffer),
                  NULL
                  );

    if (count != 0) {
        printf("%s\n", errorBuffer);
    } else {
        printf("Format message failed.  Error: %d\n", GetLastError());
    }
}

VOID
PrintInquiryData(PCHAR  DataBuffer)
{
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PSCSI_INQUIRY_DATA inquiryData;
    ULONG i,
          j;

    adapterInfo = (PSCSI_ADAPTER_BUS_INFO) DataBuffer;
    
    printf("Bus\n");
    printf("Num TID LUN Claimed String                       Inquiry Header\n");
    printf("--- --- --- ------- ---------------------------- -----------------------\n");

    for (i = 0; i < adapterInfo->NumberOfBuses; i++) {
       inquiryData = (PSCSI_INQUIRY_DATA) (DataBuffer +
          adapterInfo->BusData[i].InquiryDataOffset);
       while (adapterInfo->BusData[i].InquiryDataOffset) {
          printf(" %d   %d  %3d    %s    %.28s ",
             i,
             inquiryData->TargetId,
             inquiryData->Lun,
             (inquiryData->DeviceClaimed) ? "Y" : "N",
             &inquiryData->InquiryData[8]);
          for (j = 0; j < 8; j++) {
             printf("%02X ", inquiryData->InquiryData[j]);
             }
          printf("\n");
          if (inquiryData->NextInquiryDataOffset == 0) {
             break;
             }
          inquiryData = (PSCSI_INQUIRY_DATA) (DataBuffer +
             inquiryData->NextInquiryDataOffset);
          }
       }

    printf("\n\n");
}


PUCHAR
AllocateAlignedBuffer(ULONG size, ULONG Align)
{
    PUCHAR ptr;

    UINT_PTR    Align64 = (UINT_PTR)Align;
    
    if (!Align) {
       ptr = (PUCHAR)malloc(size);
       }
    else {
       ptr = (PUCHAR)malloc(size + Align);
       ptr = (PUCHAR)(((UINT_PTR)ptr + Align64) & ~Align64);
       }
    if (ptr == NULL) {
       printf("Memory allocation error.  Terminating program\n");
       exit(1);
       }
    else {
       return ptr;
       }
}


VOID
PrintStatusResults(
    BOOL status,DWORD returned,PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb,
    ULONG length)
{
    ULONG errorCode;

    if (!status ) {
       printf( "Error: %d  ",
          errorCode = GetLastError() );
       PrintError(errorCode);
       return;
       }
    if (psptwb->spt.ScsiStatus) {
       PrintSenseInfo(psptwb);
       return;
       }
    else {
       printf("Scsi status: %02Xh, Bytes returned: %Xh, ",
          psptwb->spt.ScsiStatus,returned);
       printf("Data buffer length: %Xh\n\n\n",
          psptwb->spt.DataTransferLength);
//       PrintDataBuffer((PUCHAR)psptwb,length);
       }
}

VOID
PrintSenseInfo(PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb)
{
    int i;

    printf("Scsi status: %02Xh\n\n",psptwb->spt.ScsiStatus);
    if (psptwb->spt.SenseInfoLength == 0) {
       return;
       }
    printf("Sense Info -- consult SCSI spec for details\n");
    printf("-------------------------------------------------------------\n");
    for (i=0; i < psptwb->spt.SenseInfoLength; i++) {
       printf("%02X ",psptwb->ucSenseBuf[i]);
       }
    printf("\n\n");
}


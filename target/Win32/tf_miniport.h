#ifndef _TF_MINIPORT_SCSI
#define _TF_MINIPORT_SCSI

int miniport_tfSCSICommand(struct tfCommand *cmd);
void *miniport_tfInit();
void miniport_tfCleanup(void *handle);
void *miniport_tfAttach(void *handle, struct Session *ses);
void miniport_tfDetach(void *handle, void *prot);


#ifdef NTDDK

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


#define DEVICE_LEN		100

struct miniportData {
	int ScsiPortNumber;
	int PathId;
	int TargetId;
	int Lun;	

	int ReadOnly;

    HANDLE fileHandle;
	IO_SCSI_CAPABILITIES capabilities;
	char device[DEVICE_LEN];
};


#define IOCTL_DEVICENAME	TEXT("DeviceFileName")
#define IOCTL_SCSIPORT		TEXT("SCSIPortNumber")
#define IOCTL_PATHID		TEXT("PathID")
#define IOCTL_TARGETID		TEXT("TargetId")
#define IOCTL_LUN			TEXT("Lun")

#define IOCTL_READONLY		TEXT("ReadOnly")

#endif

#define MINIPORT_CLASS_NAME		"IOCTL"
#define MINIPORT_CLASS_INITIALIZATOR \
	{ MINIPORT_CLASS_NAME, NULL, miniport_tfInit, miniport_tfCleanup, \
		miniport_tfAttach, miniport_tfDetach, miniport_tfSCSICommand }


#endif

#ifndef _TF_LINUXSG_SCSI
#define _TF_LINUXSG_SCSI

int sglinux_tfSCSICommand(struct tfCommand *cmd);
void *sglinux_tfInit();
void sglinux_tfCleanup(void *handle);
void *sglinux_tfAttach(void *handle, struct Session *ses);
void sglinux_tfDetach(void *handle, void *prot);

int sglinux_tfSetPollingDes(struct Session *ses);
int sglinux_tfCheckPollingDes(struct Session *ses);

typedef struct _sg_linux {
	int fd;
} sglinux_data_t;


#define SG_DRIVER_NAME "SGLINUX"
#define SG_DEVICE_NAME "DeviceName"

#define SG_CLASS_INITIALIZATOR \
	{ SG_DRIVER_NAME, NULL, sglinux_tfInit, sglinux_tfCleanup, \
		sglinux_tfAttach, sglinux_tfDetach, sglinux_tfSCSICommand, NULL, NULL, \
		TF_FLAG_USE_FD_POLLING, sglinux_tfSetPollingDes, sglinux_tfCheckPollingDes }

#endif


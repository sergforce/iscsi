#ifndef _TF_ASPI_H
#define _TF_ASPI_H

typedef struct tagASPIDATA {
	int nDevice;
	HANDLE hEvent; /* sync event */
	
	HMODULE hWnaspiDll;
	DWORD (*lpGetASPI32SupportInfo)(void);
	DWORD (*lpSendASPI32Command)(LPSRB);
	BOOL (*lpGetASPI32Buffer)(PASPI32BUFF);
	BOOL (*lpFreeASPI32Buffer)(PASPI32BUFF);
	BOOL (*lpTranslateASPI32Address)(PDWORD, PDWORD);
	
} ASPIDATA, *LPASPIDATA;


typedef struct tagASPITARGETDATA {
	uint8_t	lun;
	uint8_t haId;
	uint8_t reserved; /* should be 0xff */
	uint8_t targetId;
	
	HANDLE hEvent;
} ASPITARGETDATA, *LPASPITARGETDATA;


int aspi_tfSCSICommand(struct tfCommand *cmd);
void *aspi_tfInit();
void aspi_tfCleanup(void *handle);
void *aspi_tfAttach(void *handle, struct Session *ses);
void aspi_tfDetach(void *handle, void *prot);

#define ASPIN_HAID		TEXT("HostAdapterID")
#define ASPIN_TARGETID	TEXT("Target")
#define ASPIN_LUN		TEXT("LUN")


#define ASPI_CLASS_NAME		"ASPI"
#define ASPI_CLASS_INITIALIZATOR \
	{ ASPI_CLASS_NAME, NULL, aspi_tfInit, aspi_tfCleanup, \
		aspi_tfAttach, aspi_tfDetach, aspi_tfSCSICommand }

#endif


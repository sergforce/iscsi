#ifndef __TERGET_FILTER_
#define __TERGET_FILTER_

#define RESIDUAL_UNDERFLOW		2
#define RESIDUAL_OVERFLOW		1

#define MAX_SENSE_LEN	0x040
#define MAX_CDB_LEN		0x100

#define TF_EXTRA_LEN	0x100


#define TF_CMD		0x0001
#define TF_PDATA	0x0002
#define TF_FDATA	0x0004
#define TF_NOTIFYWAIT	0x1000

struct Session;

/* target filer base */
struct tfCommand {
	int cmd;
	int stage;

	uint64_t lun;

	uint8_t *cdbBytes;
	unsigned int cdbLen;
	

	uint32_t residualReadCount;
	uint32_t writeCount;

	uint32_t readedCount;
	uint32_t writedCount;


	struct Buffer *readBuffer;
	struct Buffer *imWriteBuffer;
	struct R2T *writeBuffs;

	uint8_t response;
	uint8_t status;

	/* 0 - is ok, 1 - Overflow, 2 - Underflow, 3-255 - Reserved */
	uint8_t readUnOv;
	uint8_t writeUnOv;

	uint8_t extra[TF_EXTRA_LEN];

	uint16_t senseLen;
	uint8_t sense[MAX_SENSE_LEN];

	/* private usage */
	struct Session *ses;
	
	/*void *priv;*/
	void *targetHandle;
	void *classHandle;
};

struct tfClass {
	const char *className;
	void *handle;

/*	
	int (*tfSCSIManagement)(void *prot, int function, struct tfCommand *ref_cmd, uint64_t lun, void *priv);
*/
	void *(*tfInit)();
	void (*tfCleanup)(void *handle);
	void *(*tfAttach)(void *handle, struct Session *ses);
	void (*tfDetach)(void *handle, void *prot);

	int (*tfSCSICommand)(struct tfCommand *cmd);

/*	const char *(*tfGetModuleInfo)(int what);*/
};

#define END_OF_CLASS_INITIALIZATOR \
{ NULL, NULL, NULL, NULL, NULL, NULL, NULL }

int sesSCSICmdResponse(struct tfCommand *cmd);
int sesSCSIQueue(struct tfCommand *cmd);


#define MAX_TFCLASES	4


#endif

#ifndef _iSCSI_H_
#define _iSCSI_H_


/* version 0.1 struct formats */
typedef struct uint24 {
	uint8_t b[3];
} uint24_t;

#ifdef LITTLE_ENDIAN
#define UINT24_TO_UINT(x)  ((x.b[0] << 16) + (x.b[1] << 8) + (x.b[2]))

#define UINT_TO_UINT24(x, y)  do {  y.b[0] = (x >> 16) & 0xff; y.b[1] = (x >> 8) & 0xff; y.b[2] = (x ) & 0xff; } while (0)

typedef struct bw20 {
	uint16_t reserved;
	uint16_t connectionID;
} bw20bhs;

typedef struct bw36 {
	uint16_t parameter1;
	union {
		uint8_t asyncVCode;
		uint8_t statusDetail;
	};
	union {
		uint8_t asyncEvent;
		uint8_t statusClass;
	};
	
} bw36bhs;

typedef struct bw40 {
	union {
		uint16_t parameter3;
		uint16_t time2Retain;
	};
	union {
		uint16_t parameter2;
		uint16_t time2Wait;
	};
} bw40bhs;



#else
#define UINT24_TO_UINT(x)  ((x.b[0]) + (x.b[1] << 8) + (x.b[2]) << 16)

#define UINT_TO_UINT24(x, y)  do {  y.b[2] = (x >> 16) & 0xff; y.b[1] = (x >> 8) & 0xff; y.b[0] = (x ) & 0xff; } while (0)

typedef struct bw36 {
	union {
		uint8_t asyncEvent;
		uint8_t statusClass;
	};
	union {
		uint8_t asyncVCode;
		uint8_t statusDetail
	};
	uint16_t parameter1;
} bw36bhs;

typedef struct bw40 {
	union {
		uint16_t parameter2;
		uint16_t time2Wait;
	};
	union {
		uint16_t parameter3
		uint16_t time2Retain;
	};
} bw40bhs;

typedef struct bw20 {	
	uint16_t connectionID;
	uint16_t reserved;
} bw20bhs;

#endif

#define ISID_T_MASK					0xC0
#define ISID_A_MASK					0x3f
#define ISID_T_SHIFT				0x06


#ifdef LITTLE_ENDIAN

struct ISID_TSIH {	
	uint32_t C : 8;
	uint32_t B : 16;
	uint32_t TA : 8;

	uint16_t tsih;	
	uint16_t D;
};

#else
struct ISID_TSIH {	
	uint32_t TA : 8;
	uint32_t B : 16;
	uint32_t C : 8;
	
	uint16_t D;
	uint16_t tsih;	
};
#endif

#define BHS_ISSET(a,b) ((a & b) == b)
#define BHS_NOTSET(a,b) ((a & b) != b)

#define BHS_OPCODE_MASK				0x3f
#define BHS_OPCODE_IMMEDIATE_BIT	0x40

#define BHS_OPCODE_NOPOUT			0x00
#define BHS_OPCODE_SCSI_COMMAND		0x01
#define BHS_OPCODE_SCSI_TASKMAN		0x02
#define BHS_OPCODE_LOGIN_REQUEST	0x03
#define BHS_OPCODE_TEXT_REQUEST		0x04
#define BHS_OPCODE_SCSI_DATA_OUT	0x05
#define BHS_OPCODE_LOGOUT_REQUEST	0x06
#define BHS_OPCODE_SNACK_REQUEST	0x10

#define BHS_OPCODE_NOPIN			0x20
#define BHS_OPCODE_SCSI_RESP		0x21
#define BHS_OPCODE_LOGIN_RESPONSE	0x23
#define BHS_OPCODE_TEXT_RESPONSE	0x24
#define BHS_OPCODE_SCSI_DATA_IN		0x25
#define BHS_OPCODE_LOGOUT_RESPONSE  0x26


#define BHS_LOGIN_T_BIT				0x80
#define BHS_LOGIN_C_BIT				0x40
#define BHS_LOGIN_CSG_MSK			0x0C
#define BHS_LOGIN_NSG_MSK			0x03

#define BHS_LOGOUT_RCODE			0x7f
#define BHS_TM_FUNCTION_MASK		0x7f

#define BHS_FLAG_FINAL				0x80
#define BHS_FLAG_CONTINUE			0x40

#define BHS_SCSI_R_FLAG				0x40
#define BHS_SCSI_W_FLAG				0x20

#define BHS_SCSI_A_FLAG				0x40
#define BHS_SCSI_BO_FLAG			0x10
#define BHS_SCSI_BU_FLAG			0x08
#define BHS_SCSI_O_FLAG				0x04
#define BHS_SCSI_U_FLAG				0x02
#define BHS_SCSI_S_FLAG				0x01

#define BHS_SCSI_ATTR_MASK			0x07


struct bhsBase {
	uint8_t cmd;
	uint8_t flags;
	uint8_t res[2];

	uint8_t totalAHSLength;
	uint24_t dataSegmentLength;

	uint32_t lun[2];

	uint32_t initiatorTaskTag;
	uint32_t reserved[7];
};


struct bhsRequest {
	uint8_t cmd;
	uint8_t flags;
	uint8_t versionMax;
	uint8_t versionMin;

	uint8_t totalAHSLength;
	uint24_t dataSegmentLength;

	union {
		uint32_t lun[2];
		uint64_t lunL;
		struct {
			uint32_t lun0;
			uint32_t lun1;
		};
		struct ISID_TSIH isid_tsih;
	};

	uint32_t initiatorTaskTag;
	union {
		uint32_t refTaskTag;
		uint32_t expDataTL;
		uint32_t targetTransferTag;
		uint32_t snackTag;
		bw20bhs  l;
	};
	uint32_t cmdSN;
	uint32_t expStatSN;
	union {
		struct {
			uint32_t refCmdSN;
			union {
				uint32_t expDataSN;
				uint32_t dataSN;
			};
			union {
				uint32_t buffOffset;
				uint32_t begRun;
			};
			uint32_t runLength;
		};
		uint8_t cdbByte[16];
	};
};

struct bhsResponse {
	uint8_t cmd;
	uint8_t flags;
	union {
		uint8_t response;
		uint8_t versionMax;
	};
	union {
		uint8_t status;
		uint8_t versionActive;
	};

	uint8_t totalAHSLength;
	uint24_t dataSegmentLength;
	union {
		uint32_t lun[2];
		struct {
			uint32_t lun0;
			uint32_t lun1;
		};
		uint64_t lunL;
		struct ISID_TSIH isid_tsih;
	};

	uint32_t initiatorTaskTag;
	union {
		uint32_t snackTag;
		uint32_t targetTransferTag;
	};
	uint32_t statSN;
	uint32_t expCmdSN;
	uint32_t maxCmdSN;
	union {
		uint32_t dataSN;
		uint32_t expDataSN;
		uint32_t r2TSN;
		bw36bhs s;
	};
	union {
		uint32_t biReadResCount;
		uint32_t buffOffset;
		bw40bhs t;
	};
	union {
		uint32_t resCount;
		uint32_t desiredDataTL;
	};
};




/* Additional Header Segment (AHS) */
struct AHS {
	uint8_t AHSSpecific;
	uint8_t AHSType;
	uint16_t AHSLength;
	uint32_t ahsData[1]; 
};

/* iSCSI  PDU */
struct PDU {
	struct {
		struct bhsBase bhs; /*BHS*/
		union {
			uint32_t headerDigest;
			struct AHS ahs;
		};
		uint32_t headerDigestWAHS;
		struct AHS *next;
	};
	
	uint32_t dataSegmentLength;

	union {
		uint32_t *dataDigest;
		struct Buffer *dataSeg; 
	};
	
	uint8_t	*data;
};



#endif


#ifndef _iSCSI_PARAM_
#define _iSCSI_PARAM_

/* Auth params */
#define ISPN_AUTHMETHOD					"AuthMethod"		/*0x200*/

#define ISPN_AM_NONE	"None"			/* 0 */  /* 0x01 */
#define ISPN_AM_CHAP	"CHAP"			/* 1 */  /* 0x02 */
#define ISPN_AM_SRP		"SRP"			/* 2 */  /* 0x04 */
#define ISPN_AM_SPKM1	"SPKM1"			/* 3 */  /* 0x08 */
#define ISPN_AM_SPKM2	"SPKM2"			/* 4 */  /* 0x10 */
#define ISPN_AM_KRB5	"KRB5"			/* 5 */  /* 0x20 */

#define ISPN_AM_NONE_MASK	0x01
#define ISPN_AM_CHAP_MASK	0x02
#define ISPN_AM_SRP_MASK	0x04
#define ISPN_AM_SPKM1_MASK	0x08
#define ISPN_AM_SPKM2_MASK	0x10
#define ISPN_AM_KRB5_MASK	0x20


#define AUTH_STATCLASS_SUCCESS			0
#define AUTH_STATCLASS_REDIRECTION		1
#define AUTH_STATCLASS_INITIATOR_ERROR	2
#define AUTH_STATCLASS_TARGET_ERROR		3

struct Param {
	union {
		int ivalue;
		char *tvalue;
	};
	union {
		int avalue; /* actual value for string aliases */
		void *pvalue;
	};
};

/* Connection Only Params*/
#define ISPN_HEADERDIGEST                "HeaderDigest"				/* 0x100 */
#define ISPN_DATADIGEST                  "DataDigest"				/* 0x101 */
#define ISPN_MAXRECVDATASEGMENTLENGTH    "MaxRecvDataSegmentLength"	/* 0x102 */

#define ISP_HEADERDIGEST                0
#define ISP_DATADIGEST                  1
#define ISP_MAXRECVDATASEGMENTLENGTH	2

#define ISPN_DIG_NONE	"None"    /* 0 - default */
#define ISPN_DIG_CRC32C "CRC32C"  /* 1 */	

#define ISP_DIG_NONE				0x00
#define ISP_DIG_CRC32C				0x01

#define ISP_DIG_MASK				0x30
#define ISP_DIG_NONE_MASK			0x10
#define ISP_DIG_CRC32C_MASK			0x20


/* Session Params*/
#define ISPN_MAXCONNECTIONS		"MaxConnections"				/*  0 */
#define ISPN_SENDTARGETS		"SendTargets"					/*  1 */
#define ISPN_TARGETNAME			"TargetName"					/*  2 */
#define ISPN_INITIATORNAME		"InitiatorName"					/*  3 */
#define ISPN_TARGETALIAS		"TargetAlias"					/*  4 */
#define ISPN_INITIATORALIAS		"InitiatorAlias"				/*  5 */
#define ISPN_TARGETADDRESS		"TargetAddress"					/*  6 */
#define ISPN_TARGETPORTALGROUPTAG	"TargetPortalGroupTag"		/*  7 */
#define ISPN_INITIALR2T			"InitialR2T"					/*  8 */
#define ISPN_IMMEDIATEDATA		"ImmediateData"					/*  9 */
#define ISPN_MAXBURSTLENGTH		"MaxBurstLength"				/* 10 */
#define ISPN_FIRSTBURSTLENGTH	"FirstBurstLength"				/* 11 */
#define ISPN_DEFAULTTIME2WAIT	"DefaultTime2Wait"				/* 12 */
#define ISPN_DEFAULTTIME2RETAIN	"DefaultTime2Retain"			/* 13 */
#define ISPN_MAXOUTSTANDINGR2T  "MaxOutstandingR2T"				/* 14 */
#define ISPN_DATAPDUINORDER		"DataPDUInOrder"				/* 15 */
#define ISPN_DATASEQUENCEINORDER	"DataSequenceInOrder"		/* 16 */
#define ISPN_ERRORRECOVERYLEVEL	"ErrorRecoveryLevel"			/* 17 */
#define ISPN_SESSIONTYPE		"SessionType"					/* 18 */

#define ISP_MAXCONNECTIONS			0
#define ISP_SENDTARGETS				1
#define ISP_TARGETNAME				2
#define ISP_INITIATORNAME			3
#define ISP_TARGETALIAS				4
#define ISP_INITIATORALIAS			5
#define ISP_TARGETADDRESS			6
#define ISP_TARGETPORTALGROUPTAG	7
#define ISP_INITIALR2T				8
#define ISP_IMMEDIATEDATA			9
#define ISP_MAXBURSTLENGTH			10
#define ISP_FIRSTBURSTLENGTH		11
#define ISP_DEFAULTTIME2WAIT		12
#define ISP_DEFAULTTIME2RETAIN		13
#define ISP_MAXOUTSTANDINGR2T		14
#define ISP_DATAPDUINORDER			15
#define ISP_DATASEQUENCEINORDER		16
#define ISP_ERRORRECOVERYLEVEL		17
#define ISP_SESSIONTYPE				18


#define ISPN_ST_NORMAL				"Normal"     /* avalue:0 - is default */
#define ISPN_ST_DISCOVER			"Discovery"  /* avalue:1 */

#define ISP_ST_NORMAL				0
#define ISP_ST_DISCOVER				1

#define ISPN_NO						"No"     /* avalue:0 */
#define ISPN_YES					"Yes"    /* avalue:1 */


#define TOTAL_DEF_PARAMS	20
#define TOTAL_PARAMS_LEN	(sizeof(struct Param) * TOTAL_DEF_PARAMS)

int addParam(struct Buffer *buff, const char *param_name, const char *paran_value);
int addParamNum(struct Buffer *buff, const char *param_name, uint32_t value);

struct ParseParams {
	struct AuthProc *ap;
	struct InetConnection *ic;
	struct Param *param;

	struct Buffer *inBuff;
	struct Buffer *outBuff;

};

int parsePDUParam(struct ParseParams *prms);




#endif


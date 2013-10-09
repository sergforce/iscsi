#include "sock.h"

#include <stdio.h>
#include "iscsi.h"
#include "dcirlist.h"
#include "iscsi_mem.h"
#include "iscsi_param.h"
#include "debug/iscsi_debug.h"
#include "tf.h"
#include "conf_reader.h"
#include "iscsi_session.h"

#include "target_conf.h"

#include <string.h>

const char* SessParamNames[] = {
	ISPN_MAXCONNECTIONS, ISPN_SENDTARGETS, ISPN_TARGETNAME, ISPN_INITIATORNAME,
	ISPN_TARGETALIAS, ISPN_INITIATORALIAS, ISPN_TARGETADDRESS, 
	ISPN_TARGETPORTALGROUPTAG, ISPN_INITIALR2T, ISPN_IMMEDIATEDATA, 
	ISPN_MAXBURSTLENGTH, ISPN_FIRSTBURSTLENGTH, ISPN_DEFAULTTIME2WAIT,
	ISPN_DEFAULTTIME2RETAIN, ISPN_MAXOUTSTANDINGR2T, ISPN_DATAPDUINORDER, 
	ISPN_DATASEQUENCEINORDER, ISPN_ERRORRECOVERYLEVEL, ISPN_SESSIONTYPE,
	NULL
};

const char* ConnParamNames[] = {
	ISPN_HEADERDIGEST, ISPN_DATADIGEST, ISPN_MAXRECVDATASEGMENTLENGTH, NULL
};

const char* AuthParamNames[] = {
	ISPN_AUTHMETHOD, NULL
};

const int ASearch[] = {0x200, -1};
const int DSearch[] = {12, 13, 15, 16, 0x101, -1};
const int ESearch[] = {17, -1};
const int FSearch[] = {11, -1};
const int HSearch[] = {0x100, -1};
const int ISearch[] = {3, 5, 8, 9, -1};
const int MSearch[] = {0, 10, 14, 0x102, -1};
const int SSearch[] = {1, 18, -1};
const int TSearch[] = {2, 4, 6, 7, -1};

const int* Search[] = {
	ASearch, NULL, NULL, DSearch, ESearch, FSearch, /* A B C D E F */
	NULL, HSearch, ISearch, NULL, NULL, NULL,       /* G H I J K L */
	MSearch, NULL, NULL, NULL, NULL, NULL,          /* M N O P Q R */
	SSearch, TSearch, NULL, NULL, NULL, NULL,       /* S T U V W X */
	NULL, NULL                                      /* Y Z         */
};

int paramcmp(const char *value, const char *x) {
	int len = strlen(value);
	if ((strncmp(value, x, len)==0) && ( *(x + len) == '=' ))
		return len + 1;
	return 0;
}


#define DUMP_PARAMS


/******************************************************************************************/
/* Version 0.1 style API */
/******************************************************************************************/


static int sendTargets(struct ParseParams *prms, const char *value)
{
	int tmp;
	struct World *wrld = prms->ic->world;
	struct confElement *elem, *el;

	lockWorld(wrld);

	el = wrld->trgs;
	
	tmp = strcmp(value, "All");
	if (tmp == 0) { /* performing All targets */
		elem = el;
		do {
			addParam(prms->outBuff,	ISPN_TARGETNAME, elem->name);
			
			elem = elem->list.next;
		} while (elem != el);
		
	} else {
		/* FIXME! */
		DEBUG("FIXME! Currently I don't know how to perform another enumeration\n");
	}
	

	releaseWorld(wrld);

	return 0;
}

static int checkTargetName(struct ParseParams *prms, const char *value, const char **tptr, void **pptr)
{
	/* currenly no checking, only one target*/
	int tmp, i;
	struct confElement *elem, *el;
	struct World *wrld = prms->ic->world;
	struct tfClass *cls;
	
	*pptr = NULL;
	*tptr = NULL;

	lockWorld(wrld);
	cls = wrld->apis;
	elem = el = wrld->trgs;
	do {
		tmp = strcmp(elem->name, value);
		if (tmp == 0) { /* Target founded */
			for (i = 0; i < wrld->nApis; i++) {
				if (strcmp(wrld->apis[i].className, elem->value) == 0) {
					*tptr = elem->value;
					*pptr = (void*)elem;
					tmp = 0;
					goto leave;
				} else {
					tmp = -1;
				}
			}
		}
		
		elem = elem->list.next;
	} while (elem != el);

leave:
	releaseWorld(wrld);

	/* Selected Target not found! */
	if (tmp) {
		DEBUG1("Selected Target %s not found!\n", value);
	}
	return tmp;
}


static int parseAuthParam(struct ParseParams *prms, int index, const char *value)
{	
	struct AuthProc *auth = prms->ap;
	const char *ptr, *ptr1;

#ifdef DUMP_PARAMS
	DEBUG2("Found auth param %s = %s\n", AuthParamNames[index], value);
#endif
	switch (index) {
	case 0:
		{
			ptr = value;			
			do {
				ptr1 = strchr(ptr, ',');
				if (ptr1 == NULL) {
					ptr1 = ptr + strlen(ptr);
				}
				
				switch (ptr1 - ptr) {
				case 3: /* SRP ? */
					if (strncmp(ISPN_AM_SRP, ptr, 3) == 0) {
						auth->availAuthTypes |= ISPN_AM_SRP_MASK;
					}
					break;
				case 4: /* None, KRB5, CHAP ? */
					if (strncmp(ISPN_AM_NONE, ptr, 4) == 0) {
						auth->availAuthTypes |= ISPN_AM_NONE_MASK; break;
					} 
					if (strncmp(ISPN_AM_CHAP, ptr, 4) == 0) {
						auth->availAuthTypes |= ISPN_AM_CHAP_MASK; break;
					} 
					if (strncmp(ISPN_AM_KRB5, ptr, 4) == 0) {
						auth->availAuthTypes |= ISPN_AM_KRB5_MASK; 
					} 
					break;
				case 5: /* SPKM1, SPKM2? */
					if (strncmp(ISPN_AM_SPKM1, ptr, 5) == 0) {
						auth->availAuthTypes |= ISPN_AM_SPKM1_MASK; break;
					} 
					if (strncmp(ISPN_AM_SPKM2, ptr, 5) == 0) {
						auth->availAuthTypes |= ISPN_AM_SPKM2_MASK; 
					} 
					break;
				}
				ptr = ptr1 + 1;
			} while (*ptr1 != 0);
			
		}
	}

	return 0;
}

static int parseConnParam(struct ParseParams *prms, int index, const char *value)
{
	struct InetConnection *conn = prms->ic;
	int headerMask = 0, useCRC, tmp;
	const char *ptr, *ptr1;

	switch (index) {
	case ISP_HEADERDIGEST:
	case ISP_DATADIGEST:
		if (prms->ap->CSG > 1) {
			DEBUG("Cannot change Digests rules in FFP\n");
			return 0;
		}

		ptr = value;			
		do {
			ptr1 = strchr(ptr, ',');
			if (ptr1 == NULL) {
				ptr1 = ptr + strlen(ptr);
			}
			switch (ptr1 - ptr) {
			case 4: /* None ? */
				if (strncmp(ISPN_DIG_NONE, ptr, 4) == 0) {
					headerMask |= ISP_DIG_NONE_MASK;
				}
				break;
			case 6: /* CRC32C */
				if (strncmp(ISPN_DIG_CRC32C, ptr, 6) == 0) {
					headerMask |= ISP_DIG_CRC32C_MASK;
				}
			}
			ptr = ptr1 + 1;
		} while (*ptr1 != 0);

		
		/* TODO: Chosing preferable configuration */
		if (index == ISP_HEADERDIGEST) {	
			if ((headerMask & ISP_DIG_MASK) == (ISP_DIG_NONE_MASK | ISP_DIG_CRC32C_MASK)) {
				useCRC = ! PREF_NONE_HEADER_DIGEST;
			} else if ((headerMask & ISP_DIG_CRC32C_MASK) == ISP_DIG_CRC32C_MASK) {
				useCRC = 1;
			} else {
				useCRC = 0;
			}
		} else { 
			if ((headerMask & ISP_DIG_MASK) == (ISP_DIG_NONE_MASK | ISP_DIG_CRC32C_MASK)) {
				useCRC = ! PREF_NONE_DATA_DIGEST;
			} else if ((headerMask & ISP_DIG_CRC32C_MASK) == ISP_DIG_CRC32C_MASK) {
				useCRC = 1;
			} else {
				useCRC = 0;
			}
		}


		if (index == ISP_HEADERDIGEST) {			
			if (useCRC) {
				addParam(prms->outBuff, ISPN_HEADERDIGEST, ISPN_DIG_CRC32C);
				conn->headerDigest = ISP_DIG_CRC32C;
			} else {
				addParam(prms->outBuff, ISPN_HEADERDIGEST, ISPN_DIG_NONE);
				conn->headerDigest = ISP_DIG_NONE;
			}
		} else {
			if (useCRC) {
				addParam(prms->outBuff, ISPN_DATADIGEST, ISPN_DIG_CRC32C);
				conn->dataDigest = ISP_DIG_CRC32C;
			} else {
				addParam(prms->outBuff, ISPN_DATADIGEST, ISPN_DIG_NONE);
				conn->dataDigest = ISP_DIG_NONE;
			}
		}
		return 0;
	case ISP_MAXRECVDATASEGMENTLENGTH:
		tmp = atoi(value);
		if ((tmp < 512) || (tmp > ((2<<24) - 1))) {
			DEBUG1("Uncorect value for Digests: %d\n", tmp);
			return 0;
		}

		conn->initiatorMaxRcvDataSegment = tmp;
		return 0;
	default:
		DEBUG2("Found conn param %s = %s\n", ConnParamNames[index], value);
	}

	return 0;
}

static int parseSessParam(struct ParseParams *prms, int index, const char *value)
{
	int len;
	int tmp;
	const char *ptr;
	void *pptr;

	switch (index) {
	case ISP_MAXCONNECTIONS: /* 0 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("MaxConnections declared not in LO phase!\n");
			return 0;
		}
		tmp = atoi(value);
		if ((tmp < 1) || (tmp > 65535)) {
			DEBUG("MaxConnections bad value!\n");
			return 0;
		}
		prms->param[index].ivalue = tmp;
		return 0;

	case ISP_SENDTARGETS: /* 1 */
		{
			return sendTargets(prms, value);
		}
	case ISP_TARGETNAME: /* 2 */
		{
			if ((prms->ap == NULL) || (prms->ap->CSG > 1) || (prms->ap->firstPDU == 0)) {
				DEBUG("TargetName declared not in Login Stage!\n");
				return 0;
			}
			if (prms->param[ISP_TARGETNAME].tvalue != NULL) {
				DEBUG("TargetName redeclaration!\n");
				return 0;
			}
			if (checkTargetName(prms, value, &ptr, &pptr) == 0) {
				prms->param[ISP_TARGETNAME].tvalue = (char*)ptr;
				prms->param[ISP_TARGETNAME].pvalue = pptr;
				return 0;
			}
			return -1;
		}
	case ISP_INITIATORALIAS: /* 5*/
	case ISP_INITIATORNAME: /* 3 */
		{
			/* FIXME! bogus storage, need Initiator struct*/
			if ((prms->ap == NULL) || (prms->ap->CSG > 1) || (prms->ap->firstPDU == 0)) {
				DEBUG("InitiatorName/Alias declared not in Login Stage!\n");
				return 0;
			}
			if (prms->param[index].tvalue != NULL) {
				DEBUG("InitiatorName/Alias redeclaration!\n");
				return 0;
			}
			len = strlen(value);
			prms->param[index].tvalue = (char*)malloc(len + 1); /*********** <-FIXME!!!************/
			strncpy(prms->param[index].tvalue, value, len);
			*(prms->param[index].tvalue + len) = 0;
			return 0;
		}
	case ISP_MAXBURSTLENGTH: /* 10 */
	case ISP_FIRSTBURSTLENGTH: /* 11 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("BurstLength declared not in LO phase only!\n");
			return 0;
		}
		tmp = atoi(value);
		if ((tmp < 512) || (tmp > ((2<<24) - 1))) {
			DEBUG("BurstLength bad value!\n");
			return 0;
		}

		/* FIXME! MaxBuffSize */
		if (index == ISP_MAXBURSTLENGTH) {
			if (tmp > MAX_U_MAXBURSTLENGTH) {
				tmp = MAX_U_MAXBURSTLENGTH;
			}
		} else {
			if (tmp > MAX_U_FIRSTBURSTLENGTH) {
				tmp = MAX_U_FIRSTBURSTLENGTH;
			}
		}
		prms->param[index].ivalue = tmp;
		return 0;

		
	case ISP_DEFAULTTIME2WAIT:   /* 12 */
	case ISP_DEFAULTTIME2RETAIN: /* 13 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("DefaultTime can be changed in LO stage!\n");
			return 0;
		}
		tmp = atoi(value);
		if ((tmp < 0) || (tmp > 3600)) {
			DEBUG("DefaultTime bad value!\n");
			return 0;
		}

		prms->param[index].ivalue = tmp;
		/* not Declarative parametrs confirm it*/
		addParam(prms->outBuff, SessParamNames[index], value); /*FIXME*/
		return 0;
	case ISP_MAXOUTSTANDINGR2T:  /* 14 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("MaxOutstandingR2T can be changed in LO stage!\n");
			return 0;
		}
		tmp = atoi(value);
		if ((tmp < 1) || (tmp > 65535)) {
			DEBUG("MaxOutstandingR2T bad value!\n");
			return 0;
		}

		prms->param[index].ivalue = tmp;
		/* not Declarative parametrs confirm it*/
		addParam(prms->outBuff, SessParamNames[index], value); /*FIXME*/
		return 0;
	case ISP_INITIALR2T:          /*  8 */
	case ISP_IMMEDIATEDATA:       /*  9 */
	case ISP_DATAPDUINORDER:      /* 15 */
	case ISP_DATASEQUENCEINORDER: /* 16 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("DataSeq, DataInO, ImmDat, IR2T can be changed in LO stage only!\n");
			return 0;
		}

		tmp = strcmp(value, ISPN_NO);
		if (tmp != 0) {
			tmp = strcmp(value, ISPN_YES);
			if (tmp != 0) {
				DEBUG1("Unknown value for YES_NO: %s!\n", value);
				return 0;
			}			
			tmp = 1;
		}
		
		prms->param[index].tvalue = (tmp) ? ISPN_YES : ISPN_NO;
		prms->param[index].avalue = tmp;

		if ((index == ISP_IMMEDIATEDATA) && (tmp == 1)) {
			addParam(prms->outBuff, ISPN_IMMEDIATEDATA, ISPN_YES);
		}

		if ((index == ISP_INITIALR2T) && (tmp == 0)) {
			addParam(prms->outBuff, ISPN_INITIALR2T, ISPN_NO);
		}

		return 0;
	case ISP_ERRORRECOVERYLEVEL: /* 17 */
		if ((prms->ap == NULL) || (prms->ap->CSG > 1)) {
			DEBUG("ErrorRecoveryLevel declared not in LO phase only!\n");
			return 0;
		}
		tmp = atoi(value);
		if ((tmp < 0) || (tmp > 2)) {
			DEBUG("ErrorRecoveryLevel bad value!\n");
			return 0;
		}
		/* We currently didn't suppot EL > 0 */
		if (tmp > 0) {
			addParamNum(prms->outBuff, ISPN_ERRORRECOVERYLEVEL, 0);
		}

		return 0;

	case ISP_SESSIONTYPE: /* 18 */
		{
			if ((prms->ap == NULL) || (prms->ap->firstPDU != 1)) {
				DEBUG("SessionType declared not in leading PDU!\n");
				return 0;
			}
			tmp = strcmp(ISPN_ST_DISCOVER, value);
			if (tmp != 0) {
				tmp = strcmp(ISPN_ST_NORMAL, value);
				if (tmp != 0) {
					DEBUG1("Unknown value for SessionType: %s!\n", value);
				} 
				return 0;
			}
			/* DiscoveryType */
			prms->param[ISP_SESSIONTYPE].tvalue = ISPN_ST_DISCOVER;
			prms->param[ISP_SESSIONTYPE].avalue = 1;
			return 0;
		}
	default:		
		DEBUG2("Found sess param %s = %s\n", SessParamNames[index], value);
	}
	return 0;
}

#define PARAM_NAME 64

int parsePDUParam(struct ParseParams *prms)
{
	unsigned char fl;
	char *ptr = prms->inBuff->data;
	const int* table;
	int index, res, i, offset;
	int len = 0;
	const char *srh, *tmp;

	char buff[PARAM_NAME];

	struct SIndex {
		int (*parse)(struct ParseParams *prms, int index, const char *value);
		int offset;
		const char **table;
	} si[] = {
		{ parseAuthParam, 0x200, AuthParamNames },
		{ parseConnParam, 0x100, ConnParamNames },
		{ parseSessParam, 0x000, SessParamNames }
	};
	
	while (ptr < (char *)(prms->inBuff->data + prms->inBuff->length)) {
		
		fl = *ptr - 'A';
		if ((fl > 26) /*|| (fl < 0)*/) { 
			DEBUG("Uncorrect parameter\n");
			return -1;
		}
		table = Search[fl];
		if (table == NULL) {
			/*DEBUG("Unknown parameter\n");*/
			tmp = strchr(ptr, '=');
			if (PARAM_NAME > (tmp - ptr)) {
				strncpy(buff, ptr, tmp - ptr);
				buff[tmp - ptr] = 0;
				addParam(prms->outBuff, buff, "NotUnderstood");
				len = 0;
			}			
			goto next;
		}
		
		while ((index = *table) != -1) {
			for (i = 0; i < (sizeof(si) / sizeof(si[0])); i++) {
				offset = si[i].offset;
				if (index >= offset) {
					srh = si[i].table[index - offset];
					if ((len = paramcmp(srh, ptr)) > 0) {
						res = si[i].parse(prms, index - offset, ptr + len);
						if (res < 0) {
							return -1;
						} else {
							goto next;
						}
					}
				}
			}
			table++;
		}

		if (index == -1)  { /* NotUnderstood */
			tmp = strchr(ptr, '=');
			if (PARAM_NAME > (tmp - ptr)) {
				strncpy(buff, ptr, tmp - ptr);
				buff[tmp - ptr] = 0;
				addParam(prms->outBuff, buff, "NotUnderstood");
				len = 0;
			}
			
			goto next;	
		}
		return -2;
		
next:
		ptr += len;
		ptr += strlen(ptr) + 1;
	}
	return 0;
}

#define MAXLEN_INT	16
int addParamNum(struct Buffer *buff, const char *param_name, uint32_t value)
{
	char buffer[MAXLEN_INT];
	/* itoa(value, buffer, 10); */
	snprintf(buffer, MAXLEN_INT, "%d", value);
	return addParam(buff, param_name, buffer);
}

int addParam(struct Buffer *buff, const char *param_name, const char *paran_value)
{
	int res = snprintf(buff->data + buff->length, buff->maxLength - buff->length,
		"%s=%s", param_name, paran_value);

	if (res > 0) {
		buff->length += res;
		if (buff->maxLength - buff->length > 1) {
			*(buff->data + buff->length) = 0;
			buff->length++;
		} else {
			return -1;
		}
	}
	return res;
}






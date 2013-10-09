#ifndef _iSCSI_DEBUG_
#define _iSCSI_DEBUG_


#ifdef _DEBUG2
struct PDU;

int dumpPDUDev(struct PDU *pdu, FILE *dev);
int initDebug(FILE* fl);

extern FILE* debugFileOutput;


#define DEBUG(x) do { fprintf(debugFileOutput, x); fflush(debugFileOutput); } while (0)
#define DEBUG1(x, y) do { fprintf(debugFileOutput, x, y); fflush(debugFileOutput);} while (0)
#define DEBUG2(x, y, z) do { fprintf(debugFileOutput, x, y, z); fflush(debugFileOutput);} while (0)
#define DEBUG3(x, y, z, p) do { fprintf(debugFileOutput, x, y, z, p); fflush(debugFileOutput);} while (0)
#define DEBUG4(x, y, z, p, q) do { fprintf(debugFileOutput, x, y, z, p, q); fflush(debugFileOutput);} while (0)

#define dumpPDU(x) dumpPDUDev(x, debugFileOutput)

#else

#define DEBUG(x) 
#define DEBUG1(x, y) 
#define DEBUG2(x, y, z)
#define DEBUG3(x, y, z, p)
#define DEBUG4(x, y, z, p, q)

#define dumpPDU(x)
#endif

#endif

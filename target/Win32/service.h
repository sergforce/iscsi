#ifndef _SERVICE_H_
#define _SERVICE_H_

#ifndef _INT_SERVICE_H_
extern HANDLE hRunningService;
#endif

/* Make program as Win32 Service Application othewise common console */
#ifndef _DEBUG2
#define _SERVICE
#endif

#endif

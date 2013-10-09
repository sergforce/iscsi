#include <windows.h>

#include <stdio.h>
#include "../debug/iscsi_debug.h"

#define _INT_SERVICE_H_

#include "service.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

int targetTest(void);


#ifdef _SERVICE

SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   ServiceStatusHandle;
HANDLE					hRunningService,
						hListenRoutine;
DWORD					dwStaringError;

#define SERVICE_NAME	"iSCSI Target Service"

DWORD ServiceInitialization(DWORD   argc, LPTSTR  *argv, DWORD *specificError) 
{ 
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2,2), &wsaData);

	if (err != 0) {
		*specificError = 0x100;
		return -1;
	}

	return NO_ERROR;
}


VOID WINAPI ServiceCtrlHandler (DWORD Opcode) 
{ 
   DWORD status; 
 
   switch(Opcode) 
   { 
      case SERVICE_CONTROL_PAUSE: 
      // Do whatever it takes to pause here. 
         ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
         break; 
 
      case SERVICE_CONTROL_CONTINUE: 
      // Do whatever it takes to continue here. 
         ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
         break; 
 
      case SERVICE_CONTROL_STOP: 
      // Do whatever it takes to stop here. 
         ServiceStatus.dwWin32ExitCode = 0; 
         ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
         ServiceStatus.dwCheckPoint    = 0; 
         ServiceStatus.dwWaitHint      = 0; 

         if (!SetServiceStatus (ServiceStatusHandle, 
           &ServiceStatus))
         { 
            status = GetLastError(); 
            DEBUG1("SetServiceStatus error %ld\n", status); 
         } 
 
         DEBUG("Leaving MyService \n"); 
         return; 
 
      case SERVICE_CONTROL_INTERROGATE: 
      // Fall through to send current status. 
         break; 
 
      default: ;
   } 
 
   // Send current status. 
   if (!SetServiceStatus (ServiceStatusHandle,  &ServiceStatus)) 
   { 
      status = GetLastError(); 
      DEBUG1("SetServiceStatus error %ld\n", status); 
   } 
   return; 
}

DWORD WINAPI listenRoutine(LPVOID data)
{
	dwStaringError = 0;
	dwStaringError = targetTest();
	SetEvent(hRunningService);
	return 0;
}


void WINAPI ServiceStart (DWORD argc, LPTSTR *argv) 
{ 
    DWORD status; 
    DWORD specificError; 
 
    ServiceStatus.dwServiceType        = SERVICE_WIN32; 
    ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | 
        SERVICE_ACCEPT_PAUSE_CONTINUE; 
    ServiceStatus.dwWin32ExitCode      = 0; 
    ServiceStatus.dwServiceSpecificExitCode = 0; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0; 
 
    ServiceStatusHandle = RegisterServiceCtrlHandler( 
        SERVICE_NAME, 
        ServiceCtrlHandler); 
 
    if (ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) 
    { 
        DEBUG1("RegisterServiceCtrlHandler failed %ld\n", GetLastError()); 
        return; 
    } 
 
    // Initialization code goes here. 
    status = ServiceInitialization(argc,argv, &specificError); 
	hRunningService = CreateEvent(NULL, FALSE, FALSE, NULL);
	hListenRoutine = CreateThread(NULL, 0, listenRoutine, NULL, 0, NULL);

	WaitForSingleObject(hRunningService, INFINITE);

    // Handle error condition 
    if ((status != NO_ERROR) || (dwStaringError != 0))
    { 
        ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
        ServiceStatus.dwCheckPoint         = 0; 
        ServiceStatus.dwWaitHint           = 0; 
        ServiceStatus.dwWin32ExitCode      = status; 
        ServiceStatus.dwServiceSpecificExitCode = specificError; 
 
        SetServiceStatus (ServiceStatusHandle, &ServiceStatus); 
        return; 
    } 
 
    // Initialization complete - report running status. 
    ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0; 
 
    if (!SetServiceStatus (ServiceStatusHandle, &ServiceStatus)) 
    { 
        status = GetLastError(); 
        DEBUG1("SetServiceStatus error %ld\n", status); 
    } 
 
    // This is where the service does its work. 
	//targetTest();


    DEBUG("Returning the Main Thread \n"); 
 
    return; 
} 


int main(int argc, char* argv[]) 
{ 
   SERVICE_TABLE_ENTRY   DispatchTable[] = { 
      { SERVICE_NAME,	ServiceStart}, 
      { NULL,           NULL} 
   }; 
#ifdef _DEBUG2	
	initDebug(stdout);
#endif

   if (!StartServiceCtrlDispatcher(DispatchTable)) { 
      DEBUG1("StartServiceCtrlDispatcher (%ld)\n", GetLastError()); 
   } 

   return 0;
} 

#else
/* stanalone console version */

int main(int argc, char* argv[])
{	
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2,2), &wsaData);

	if (err != 0) {
		return -1;
	}

#ifdef _DEBUG2	
	initDebug(stdout);
#endif
	targetTest();
	return 0;
}


#endif

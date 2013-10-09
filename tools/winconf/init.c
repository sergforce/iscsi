// init.cpp : Defines the entry point for the application.
//

/* #include "stdafx.h" */

/* Internet Explorer 4.0 or higher required */
#define _WIN32_IE		0x0400

#include "../../target/sock.h"
#include "resource.h"

#include <tchar.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _MSC_VER
#include <ddk/ntddscsi.h>
#else
#include <devioctl.h>
#include <ntddscsi.h>
#endif
#include <stddef.h>

#include "exwin.h"

#include "../../target/dcirlist.h"
#include "../../target/iscsi_mem.h"
#include "../../target/conf_reader.h"

#include "../../target/Win32/wnaspi/srbcmn.h"
#include "../../target/Win32/wnaspi/srb32.h"


#ifdef _MSC_VER
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "advapi32.lib")
#endif

#define BUFF_SIZE		256
#define INQUIRY_LEN		36

/* ASPI */
void *aspi_init(struct configuration *cnf);
void aspi_free(struct configuration *cnf, void *data);
int aspi_InquiryBuses(void *data, HWND hCtrl, HWND hDlg);
int aspi_CheckSel(void *data, LPARAM lParam);
int apsi_CheckDevice(void *data, struct confElement *dev, void *trg, char *inquiryInfo);
int apsi_addTarget(void *data, struct configuration *cnf, struct confElement *parent, void *trg);
INT_PTR CALLBACK aspi_PropDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* IOCTL */
void *ioctl_init(struct configuration *cnf);
void ioctl_free(struct configuration *cnf, void *data);
int ioctl_InquiryBuses(void *data, HWND hCtrl, HWND hDlg);
int ioctl_CheckSel(void *data, LPARAM lParam);
int ioctl_CheckDevice(void *data, struct confElement *dev, void *trg, char *inquiryInfo);
int ioctl_addTarget(void *data, struct configuration *cnf, struct confElement *parent, void *trg);
INT_PTR CALLBACK ioctl_PropDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


typedef struct _APIDIALOGS {
	const char *name;
	UINT uPropDlgID;
	DLGPROC lpPropDlg;
	void *intData;

	void *(*init)(struct configuration *cnf);
	void (*free)(struct configuration *cnf, void *data);
	
	/* Enumerating */
	int (*InquiryBuses)(void *data, HWND hCtrl, HWND hDlg);
	int (*CheckSel)(void *data, LPARAM lParam);
	int (*CheckDevice)(void *data, struct confElement *dev, void *trg, char *inquiryInfo);

	/* Adding device */
	int (*addTarget)(void *data, struct configuration *cnf, struct confElement *parent, void *trg);


} APIDIALOGS, *LPAPIDIALOGS;

APIDIALOGS ad_template[] = {
	{"ASPI", IDD_PROP_ASPI, aspi_PropDlg, NULL, aspi_init, aspi_free, aspi_InquiryBuses, 
		aspi_CheckSel, apsi_CheckDevice, apsi_addTarget},
	{"IOCTL", IDD_PROP_IOCTL, ioctl_PropDlg, NULL, ioctl_init, ioctl_free, ioctl_InquiryBuses, 
		ioctl_CheckSel, ioctl_CheckDevice, ioctl_addTarget},

	{NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL}
};

APIDIALOGS ad[3];

struct PropertyData {
	HINSTANCE hInstance;
	struct configuration cnf;
	/*	
	int serviceState;
	int nTargets;
	*/
	LPAPIDIALOGS lpad;

	/* adding new target */
	struct confElement *newTarget;

	/* temp data for dialog box */
	void *tempData[4];
	char tempBuff[BUFF_SIZE];
};

typedef struct tagDLGPROPDATA {
	struct confElement *el;
	void *intData;
	struct configuration *cnf;
} DLGPROPDATA, *LPDLGPROPDATA;

INT_PTR CALLBACK mainDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT DoPropSheet(HWND main, HINSTANCE hInst, struct PropertyData *pd);
INT_PTR CALLBACK addTargetDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK serviceDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static int InstallMyService(void);
static int DeleteMyService(void);


#define CheckBox_SetCheck(w,x)	SendMessage(w, BM_SETCHECK, (x) ? BST_CHECKED : BST_UNCHECKED, 0)
#define CheckBox_GetCheck(w)	(SendMessage(w, BM_GETCHECK, 0, 0) == BST_CHECKED)


int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	struct PropertyData pd;
	int j = 0, i;

	if (strstr(lpCmdLine, "--installservice") != NULL) {
		return InstallMyService();
	} else if (strstr(lpCmdLine, "--deleteservice") != NULL) {
		return DeleteMyService();
	}
	
	pd.hInstance = hInstance;
 	pd.lpad = ad;

	initConfiguration(&pd.cnf);

	for (i = 0; i < sizeof(ad_template) / sizeof(ad_template[0]); i++) {
		if (ad_template[i].name != NULL) {
			ad_template[i].intData = ad_template[i].init(&pd.cnf);
			if (ad_template[i].intData != NULL) {
				memcpy(&ad[j], &ad_template[i], sizeof(ad_template[0]));
				j++;
			}
		} else {
			memset(&ad[j], 0, sizeof(ad_template[0]));
		}
	}

	DoPropSheet(NULL, hInstance, &pd);
	return 0;
}


static void CenterWindow(HWND hwnd)
{
	HDC hdc;
	int sx, sy, cx, cy;
	RECT rc;
	
	hdc = GetDC(hwnd);
	sx = GetDeviceCaps(hdc, HORZRES);
	sy = GetDeviceCaps(hdc, VERTRES);
	ReleaseDC(hwnd, hdc);
	GetWindowRect(hwnd, &rc);
	cy = rc.bottom - rc.top;
	cx = rc.right - rc.left;
	MoveWindow(hwnd, (sx - cx) / 2, (sy - cy) / 2, cx, cy, FALSE);	
}


INT DoPropSheet(HWND main, HINSTANCE hInst, struct PropertyData *pd)
{
    PROPSHEETPAGE psp[2];
    PROPSHEETHEADER psh;

    psp[0].dwSize = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags = PSP_USETITLE;
    psp[0].hInstance = hInst;
    psp[0].pszTemplate = MAKEINTRESOURCE(IDD_CONFIG);
    psp[0].pszIcon = NULL;
    psp[0].pfnDlgProc = mainDlg;
    psp[0].pszTitle = MAKEINTRESOURCE(IDS_CONFIG);
    psp[0].lParam = (LPARAM)pd;
    psp[0].pfnCallback = NULL;

    psp[1].dwSize = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags = PSP_USETITLE;
    psp[1].hInstance = hInst;
    psp[1].pszTemplate = MAKEINTRESOURCE(IDD_SERVICE);
    psp[1].pszIcon = NULL;
    psp[1].pfnDlgProc = serviceDlg;
    psp[1].pszTitle = MAKEINTRESOURCE(IDS_SERVICE);
    psp[1].lParam = (LPARAM)pd;
    psp[1].pfnCallback = NULL;

    psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSP_USETITLE ;
    psh.hwndParent = main;
    psh.hInstance = hInst;
    psh.pszCaption = MAKEINTRESOURCE(IDS_PROP_DBCAPTION);
    psh.nStartPage = 0;
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;
    psh.pfnCallback = NULL;
	
	return PropertySheet(&psh);	
}


static int Message(HWND hwndDlg, UINT msg, UINT cap, UINT flags)
{
	CHAR buff[BUFF_SIZE];
	CHAR buffCaption[BUFF_SIZE];

	LoadString(GetModuleHandle(NULL), msg, buff, BUFF_SIZE);
	LoadString(GetModuleHandle(NULL), cap, buffCaption, BUFF_SIZE);

	return MessageBox(hwndDlg, buff, buffCaption, flags);
}

static int ErrorMessage(HWND hwndDlg, UINT msg, UINT cap)
{
	return Message(hwndDlg, msg, cap, MB_OK | MB_ICONSTOP);
}

static int NotifyMessage(HWND hwndDlg, UINT msg, UINT cap)
{
	return Message(hwndDlg, msg, cap, MB_OK | MB_ICONINFORMATION);
}
/********************************************************************/
/* Service control function */

#define SERVICE_NAME		TEXT("iSCSITarget")
#define SERVICE_DESCRIPTION	TEXT("iSCSI Target open daemon")
#define IMAGE_PATH_KEY		TEXT("ServiceFullPath")

static int InstallMyService(void)
{
	CHAR buffer[2048];
	HKEY pKey;
	DWORD type, count = 2048;
	SC_HANDLE schSCManager, schService;
	
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, MAIN_TREE, 0, KEY_READ, &pKey) != ERROR_SUCCESS) {
		return -1;
	}
		
	if (RegQueryValueEx(pKey, IMAGE_PATH_KEY, NULL, &type, (PUCHAR)buffer, &count) != ERROR_SUCCESS) {		
		RegCloseKey(pKey);
		return -1;
	}

	RegCloseKey(pKey);

	if (type != REG_SZ) {
		return -1;
	}

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL) {
		return -1;
	}

	schService = CreateService( 
        schSCManager,
        SERVICE_NAME,
        SERVICE_DESCRIPTION,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        buffer,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);
	
	CloseServiceHandle(schSCManager);
	if (schService == NULL) {
		return -1;
	}
	
	CloseServiceHandle(schService);
	return 0;
}

static int DeleteMyService(void)
{
	SC_HANDLE schSCManager, schService;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL) {
		return -1;
	}

    schService = OpenService(schSCManager, SERVICE_NAME, DELETE);
    if (schService == NULL) { 
		CloseServiceHandle(schSCManager);
        return -1;
    }
	
	CloseServiceHandle(schSCManager);

    if (! DeleteService(schService) )  {

		return -1;
	}
	CloseServiceHandle(schService);
	return 0;
}


static const char *serviceStates[] = {
	"ERROR!",							/* 0 */
	"The service is not running",		/* 1 */
	"The service is starting",			/* 2 */
	"The service is stopping",			/* 3 */
	"The service is running",			/* 4 */
	"The service continue is pending",	/* 5 */
	"The service pause is pending",		/* 6 */
	"The service is paused"				/* 7 */
};

/* tempData[0] - SC_HANDLE schSCManager */
/* tempData[1] - SC_HANDLE schService */
static int serviceDlgFillState(HWND hwndDlg, struct PropertyData *lppd)
{
	SERVICE_STATUS_PROCESS ssStatus; 
	DWORD dwBytesNeeded;
	BOOL bEnableStart,
		 bEnableStop;

	if (!QueryServiceStatusEx((SC_HANDLE)lppd->tempData[1], SC_STATUS_PROCESS_INFO, (PUCHAR)&ssStatus, 
			sizeof(SERVICE_STATUS_PROCESS),	&dwBytesNeeded )) {
        return -1; 
    }

	SetDlgItemText(hwndDlg, IDC_STATUS, 
		serviceStates[ (ssStatus.dwCurrentState > 7) ? 0 : ssStatus.dwCurrentState ]);

	switch (ssStatus.dwCurrentState) {
	case 1:
		bEnableStart = TRUE; bEnableStop = FALSE; break;
	case 2:
		bEnableStart = FALSE; bEnableStop = FALSE; break;
	case 3:
		bEnableStart = FALSE; bEnableStop = FALSE; break;
	case 4:
		bEnableStart = FALSE; bEnableStop = TRUE; break;
	case 5:
		bEnableStart = FALSE; bEnableStop = FALSE; break;
	case 6:
		bEnableStart = FALSE; bEnableStop = FALSE; break;
	case 7:
		bEnableStart = FALSE; bEnableStop = FALSE; break;
	default:
		return -1;
	}

	EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), bEnableStop);
	EnableWindow(GetDlgItem(hwndDlg, IDC_START), bEnableStart);
	return 0;
}

static int serviceDlgInit(HWND hwndDlg, struct PropertyData *lppd)
{
	SC_HANDLE schSCManager, schService;
	int res;

	lppd->tempData[2] = NULL;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL) {
		return -1;
	}

    schService = OpenService(schSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (schService == NULL) { 
        return -1;
    }

	lppd->tempData[0] = (void*)schSCManager;
	lppd->tempData[1] = (void*)schService;
	lppd->tempData[2] = (void*)1;

	res = serviceDlgFillState(hwndDlg, lppd);
	if (res == -1) {
		EnableWindow(GetDlgItem(hwndDlg, IDC_START), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_STATUS), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_TEXT_STATUS), FALSE);
	}
	return res;
}

static int serviceDlgClean(HWND hwndDlg, struct PropertyData *lppd)
{
	if (lppd->tempData[2]) {
		CloseServiceHandle((SC_HANDLE)lppd->tempData[0]);
		CloseServiceHandle((SC_HANDLE)lppd->tempData[1]);
	}
	return 0;
}

static int serviceDlgStartService(HWND hwndDlg, struct PropertyData *lppd)
{
	if (!StartService((SC_HANDLE)lppd->tempData[1], 0, NULL)) {
		ErrorMessage(hwndDlg, IDS_ECANTSTARTSERV, IDS_SERVCONTROL);
		return -1;
    }

	EnableWindow(GetDlgItem(hwndDlg, IDC_START), FALSE);
	return 0;
}


static int serviceDlgStopService(HWND hwndDlg, struct PropertyData *lppd)
{
	SERVICE_STATUS ss;

	if (!ControlService((SC_HANDLE)lppd->tempData[1], SERVICE_CONTROL_STOP, &ss)) {
		ErrorMessage(hwndDlg, IDS_ECANTSTOPSERV, IDS_SERVCONTROL);
		return -1;
    }

	EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), FALSE);
	return 0;
}

INT_PTR CALLBACK serviceDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	INT id;
	LPPROPSHEETPAGE ps;
	LPNMHDR nhdr;

	struct PropertyData *lppd = (struct PropertyData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (uMsg) {
	case WM_INITDIALOG:
		ps = (LPPROPSHEETPAGE)lParam;
		SetWindowLongPtr (hwndDlg, GWLP_USERDATA, ps->lParam);
		lppd = (struct PropertyData *)ps->lParam;
		/*if (serviceDlgInit(hwndDlg, lppd) != -1) {
			SetTimer(hwndDlg, 0, 500, NULL);
		}*/
		return TRUE;
	case WM_TIMER:
		serviceDlgFillState(hwndDlg, lppd);
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);
		switch (id) {
		case IDC_START:
			serviceDlgStartService(hwndDlg, lppd);
			return TRUE;

		case IDC_STOP:
			serviceDlgStopService(hwndDlg, lppd);
			return TRUE;
		}
		
		return FALSE;
	case WM_DESTROY:
		serviceDlgClean(hwndDlg, lppd);
		return TRUE;
	case WM_NOTIFY:
		nhdr = (LPNMHDR) lParam;
		if ((nhdr->code == PSN_KILLACTIVE )) {
			KillTimer(hwndDlg, 0);
			serviceDlgClean(hwndDlg, lppd);
			return TRUE;
		}
		if ((nhdr->code == PSN_SETACTIVE )) {
			if (serviceDlgInit(hwndDlg, lppd) != -1) {
				SetTimer(hwndDlg, 0, 500, NULL);
			}			return TRUE;
		}
		
	}

	return FALSE;
}
/********************************************************************/
/* Target List Functions */

static int initTargetsDialog(HWND hwndDlg, struct PropertyData *pd)
{
	LVCOLUMN col;
	HWND hList = GetDlgItem(hwndDlg, IDC_TARGETLIST);
	CHAR buff[BUFF_SIZE];
	struct confElement *el1, *el2;
	int i = 0;

	LVITEM target;

	/* Initializing ListView */
	col.mask = LVCF_TEXT | LVCF_WIDTH;
	col.pszText = buff;

	col.cx = 220;	
	LoadString(pd->hInstance, IDS_TARGETNAME, buff, BUFF_SIZE);
	ListView_InsertColumn(hList, 0, &col);

	col.cx = 100;
	LoadString(pd->hInstance, IDS_APINAME, buff, BUFF_SIZE);
	ListView_InsertColumn(hList, 1, &col);

	/* Adding known targets */
	el1 = findMain(&pd->cnf, TARGETS);
	if (el1 == NULL) {
		return 0;
	}
	
	el2 = el1 = el1->childs;
	if (el1 == NULL) {
		return 0;
	}

	target.mask =  LVIF_TEXT | LVIF_PARAM;
	target.iSubItem = 0;
	do {
		target.pszText = el1->name;
		target.lParam = (LPARAM)el1;
		target.iItem = i++;

		ListView_InsertItem(hList, &target);
		ListView_SetItemText(hList, target.iItem, 1, el1->value);

		el1 = (struct confElement *)el1->list.next;
	} while (el1 != el2);


	return i;
}


LPAPIDIALOGS findAD(struct PropertyData *pd, char *name)
{
	LPAPIDIALOGS lpad = pd->lpad;
	while (lpad->name != NULL) {
		if (strcmp(lpad->name, name) == 0) {
			return lpad;
		}
		lpad++;
	}
	return NULL;
}

static int DoPropertyTarget(HWND hwndDlg, struct confElement *el, struct PropertyData *pd)
{
	LPAPIDIALOGS lpad = findAD(pd, el->value);
	CHAR buff[BUFF_SIZE];
	DLGPROPDATA dlpd;
	int res;

	if (lpad) {
		dlpd.el = el;
		dlpd.intData = lpad->intData;
		dlpd.cnf = &pd->cnf;

		res = DialogBoxParam(pd->hInstance, MAKEINTRESOURCE(lpad->uPropDlgID), hwndDlg, 
			lpad->lpPropDlg, (LPARAM)&dlpd);
		if (res == IDOK) {
			PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
		}
		return 0;
	}

	LoadString(pd->hInstance, IDS_ENDRV, buff, BUFF_SIZE);
	MessageBox(hwndDlg, buff, NULL, MB_OK);
	return -1;
}

static int DoDeleteTarget(HWND hwndDlg, struct confElement *el, struct PropertyData *pd, 
												int item, HWND hCtrl)
{
	ListView_DeleteItem(hCtrl, item);
	removeElement(&pd->cnf, el);
	if (ListView_GetItemCount(hCtrl) == 0) {	
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_DELETE), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_PROPERTY), FALSE);
	}
	
	PropSheet_Changed(GetParent(hwndDlg), hwndDlg);

	return 0;
}

static int DoAddTarget(HWND hwndDlg, struct PropertyData *pd)
{
	LVITEM target;
	HWND hList;
	int res = DialogBoxParam(pd->hInstance, MAKEINTRESOURCE(IDD_ADDDEVICE), hwndDlg,
				addTargetDlg, (LPARAM)pd);

	if (res == IDOK) {
		hList = GetDlgItem(hwndDlg, IDC_TARGETLIST);

		target.mask =  LVIF_TEXT | LVIF_PARAM;
		target.iSubItem = 0;
		target.pszText = pd->newTarget->name;
		target.lParam = (LPARAM)pd->newTarget;
		target.iItem =  ListView_GetItemCount(hList);
		
		ListView_InsertItem(hList, &target);
		ListView_SetItemText(hList, target.iItem, 1, pd->newTarget->value);

		PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
		return 0;
	}

	return -1;
}


INT_PTR CALLBACK mainDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	INT id;
	LPPROPSHEETPAGE ps;
	LPNMHDR hdr;
	LPNMITEMACTIVATE item;
	LVITEM itm;
	HWND hCtrl;
	LPAPIDIALOGS lpad;
	int i;

	struct PropertyData *lppd = (struct PropertyData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (uMsg) {
	case WM_INITDIALOG:
		ps = (LPPROPSHEETPAGE)lParam;
		SetWindowLongPtr (hwndDlg, GWLP_USERDATA, ps->lParam);
		lppd = (struct PropertyData *)ps->lParam;
		initTargetsDialog(hwndDlg, lppd);

		CenterWindow(GetParent(hwndDlg));
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);

		switch (id) {
		case IDC_ADDTRG:
			DoAddTarget(hwndDlg, lppd);
			return TRUE;
		case IDC_PROPERTY:
		case IDC_DELETE:
		case IDC_CHECK:
			hCtrl = GetDlgItem(hwndDlg, IDC_TARGETLIST);
			itm.iItem = ListView_GetSelectionMark(hCtrl);
			if (itm.iItem == -1) {
				return TRUE;
			}
			itm.mask = LVIF_PARAM;
			itm.iSubItem = 0;
			ListView_GetItem(hCtrl, &itm);

			if (id == IDC_PROPERTY) {
				DoPropertyTarget(hwndDlg, (struct confElement *)itm.lParam, lppd);
			} else if (id == IDC_DELETE) {
				DoDeleteTarget(hwndDlg, (struct confElement *)itm.lParam, lppd, itm.iItem, hCtrl);
			} else if (id == IDC_CHECK) {
				lpad = findAD(lppd, ((struct confElement *)itm.lParam)->value);
				if (lpad == NULL) {
					ErrorMessage(hwndDlg, IDS_ENDRV, IDS_DEVTEST);
					return TRUE;
				}
				i = lpad->CheckDevice(lpad->intData, (struct confElement *)itm.lParam, NULL, NULL);
				if (i == -1) {
					ErrorMessage(hwndDlg, IDS_EDEVICEERROR, IDS_DEVTEST);
					return TRUE;
				}
				NotifyMessage(hwndDlg, IDS_DEVICEOK, IDS_DEVTEST);
				return TRUE;
			}
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hwndDlg, id);
			return TRUE;
		default:
			return FALSE;
		}
	case WM_NOTIFY:
		{
			hdr = (LPNMHDR)lParam;
			switch (wParam) {
			case IDC_TARGETLIST:
				{
					if (hdr->code == NM_DBLCLK) {
						item = (LPNMITEMACTIVATE)lParam;
						itm.mask = LVIF_PARAM;
						itm.iItem = item->iItem;
						if (itm.iItem != -1) {
							itm.iSubItem = item->iSubItem;
							ListView_GetItem(item->hdr.hwndFrom, &itm);
							DoPropertyTarget(hwndDlg, (struct confElement *)itm.lParam, lppd);
						}
						return TRUE;
					} else if (hdr->code == NM_CLICK) {
						item = (LPNMITEMACTIVATE)lParam;						
						i = (item->iItem == -1) ? 0 : 1;
						EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK), i);
						EnableWindow(GetDlgItem(hwndDlg, IDC_DELETE), i);
						EnableWindow(GetDlgItem(hwndDlg, IDC_PROPERTY), i);
						return TRUE;
					}/* else if (hdr->code == NM_RELEASEDCAPTURE) {
						//item = item;
						i = -1;
						do {

						} while (i != -1);
						return TRUE;
					
					}*/
					return FALSE;
				}
			}
			if (hdr->code == PSN_APPLY) {
				syncConfiguration(&lppd->cnf);
				return TRUE;
			}

			
			return FALSE;
		}		
	default:
		return FALSE;
	}

}

/********************************************************************/
/* Param Functions */

struct confElement *SetParamText(struct configuration *cnf, struct confElement *head, const char *paramName, const char* value)
{
	struct confElement *el;

	if (head->childs) {
		el = findElement(head->childs, paramName);
	} else {
		el = NULL;
	}
	if (el == NULL) {
		el = addElementFill(cnf, head, paramName, value);
		if (el == NULL) {
			removeElement(cnf, el);
			return NULL;
		}
	} else {
		el->cbValue = strlen(value);
		strncpy(el->value, value, VALUE_LEN);
	}
	return el;
}

struct confElement *SetParamInt(struct configuration *cnf, struct confElement *head, const char *paramName, int value)
{
	char buffer[BUFF_SIZE];
	_snprintf(buffer, BUFF_SIZE, "%d", value);
	return SetParamText(cnf, head, paramName, buffer);
}

struct confElement *GetParamInt(struct confElement *head, const char *paramName, int *value)
{
	struct confElement *el = findElement(head->childs, paramName);
	if (el) {
		if (sscanf(el->value, "%d", value) == 1) {
			return el;
		}
	}
	return NULL;
}

/*********************************************************************/
/* iSCSI specific */

static int isValidChar(int ch)
{
	if (((ch >= 'a') && (ch <= 'z')) ||
		((ch >= '0') && (ch <= '9')) ||
	/*  (ch == '_') ||  */
		(ch == '.') ||
		(ch == '-')) {
		return 0;
	}
	return -1;
}

#ifndef tolower
static int tolower(int ch)
{
	if ((ch >= 'A') && (ch <= 'Z')) {
		return (ch - 'A' + 'a');
	}
	return ch;
}
#endif

BOOL isValidTargetName(char *name)
{
	int ch;
	char *ptr = name;
	if (*name == 0) {
		return FALSE;
	}

	while ((ch = *ptr) != 0) {
		ch = tolower(ch);
		*ptr = ch;
		if (isValidChar(ch) == -1) {
			return FALSE;
		}
		ptr++;
	}
	

	return TRUE;
}

/********************************************************************/
/* Add Target Functions */

struct confElement *addTargetHead(struct configuration *cnf, char *trgName, void *trg, LPAPIDIALOGS lpad)
{
	struct confElement *mainEl;
	struct confElement *chld;
	int res;

	mainEl = findMain(cnf, TARGETS);
	if (mainEl == NULL) {
		mainEl = addMain(cnf, TARGETS, "");
		if (mainEl == NULL) {
			return NULL;
		}
	}
	chld = addElementFill(cnf, mainEl, trgName, lpad->name);
	if (chld != NULL) {
		res = lpad->addTarget(lpad->intData, cnf, chld, trg);
		if (res >= 0) {
			return chld;
		} else {
			removeElement(cnf, chld);
		}
	} 

	return NULL;	
}


static int initAddTargetDlg(HWND hwndDlg, struct PropertyData *pd)
{
	LPAPIDIALOGS lpad = pd->lpad;
	HWND hApiList = GetDlgItem(hwndDlg, IDC_API);
	int index;

	while (lpad->name != NULL) {
		index = ComboBox_AddString(hApiList, lpad->name);
		ComboBox_SetItemData(hApiList, index, (LPARAM)lpad);
		lpad++;
	}

	if (lpad) {
		index = ComboBox_SetCurSel(hApiList, 0);
		if (index == 0) {
			SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_API, LBN_SELCHANGE), 
				(LPARAM)GetDlgItem(hwndDlg, IDC_API));
		}
	}

	return 0;
}

static void addDlgEnableOkButton(HWND hwndDlg, struct PropertyData *lppd)
{
	BOOL enable;
	if ((lppd->tempData[3]) && (lppd->tempData[2])) {
		enable = TRUE;
	} else {
		enable = FALSE;
	}

	EnableWindow(GetDlgItem(hwndDlg, IDOK), enable);
}

INT_PTR CALLBACK addTargetDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	INT id;
	HWND hCtrl;
	/*LPARAM data;*/
	LPNMHDR hdr;
	TVITEM tvi;
	BOOL res;
	
	int item;
	struct PropertyData *lppd = (struct PropertyData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	
	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowLongPtr (hwndDlg, GWLP_USERDATA, lParam);
		lppd = (struct PropertyData *)lParam;
		lppd->tempData[3] = lppd->tempData[2] = lppd->tempData[1] = lppd->tempData[0] = NULL;		

		initAddTargetDlg(hwndDlg, lppd);
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);
		
		switch (id) {
		case IDC_API:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				item = ComboBox_GetCurSel((HWND)lParam);
				if (item != LB_ERR) {					
					lppd->tempData[0] = (void*)ComboBox_GetItemData((HWND)lParam, item);
					SendMessage(hwndDlg, WM_COMMAND, IDC_INQUIRY, 0);
				}
				EnableWindow(GetDlgItem(hwndDlg, IDC_INQUIRY), item != LB_ERR);
				return TRUE;
			}
			return FALSE;
		case IDC_INQUIRY:
			hCtrl = GetDlgItem(hwndDlg, IDC_DEVICE);
			TreeView_DeleteAllItems(hCtrl);
			((LPAPIDIALOGS)lppd->tempData[0])->InquiryBuses(((LPAPIDIALOGS)lppd->tempData[0])->intData,
										hCtrl, hwndDlg);
			return TRUE;
		case IDOK:
			lppd->newTarget = addTargetHead(&lppd->cnf, lppd->tempBuff, lppd->tempData[1],
													(LPAPIDIALOGS)lppd->tempData[0]);
			if (lppd->newTarget == NULL) {
				ErrorMessage(hwndDlg, IDS_ECANTCONFIGUER, IDS_TARGETCONF);
				return TRUE;
			}
		case IDCANCEL:
			EndDialog(hwndDlg, id);
			return TRUE;

		case IDC_TARGETNAME:
			if (HIWORD(wParam) == EN_CHANGE) {
				GetWindowText((HWND)lParam, lppd->tempBuff, BUFF_SIZE);
				lppd->tempData[3] = (void *)isValidTargetName(lppd->tempBuff);
				
				addDlgEnableOkButton(hwndDlg, lppd);
				return TRUE;
			}
			return FALSE;
		default:
			return FALSE;
		}
	case WM_NOTIFY:
		hdr = (LPNMHDR)lParam;
		switch (hdr->idFrom) {
		case IDC_DEVICE:				
			if (hdr->code == TVN_SELCHANGED) {
				tvi.mask = TVIF_PARAM;
				tvi.hItem = TreeView_GetSelection(hdr->hwndFrom);
				res = TreeView_GetItem(hdr->hwndFrom, &tvi);
				if (res) {
					lppd->tempData[1] = (void *)tvi.lParam;
					if (lppd->tempData[0]) {
						lppd->tempData[2] = (void *)((LPAPIDIALOGS)lppd->tempData[0])->CheckSel(
										((LPAPIDIALOGS)lppd->tempData[0])->intData, tvi.lParam);
					}

					addDlgEnableOkButton(hwndDlg, lppd);
				}
				return TRUE;
			}
			return FALSE;		
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
	
}


/**********************************************************************************/
/* ASPI probing and functions */

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


void *aspi_init(struct configuration *cnf)
{
	LPASPIDATA lpaspi;
	HMODULE hDLL;
	DWORD dwGetASPI;

	lpaspi = (LPASPIDATA)malloc(sizeof(struct tagASPIDATA));
	if (lpaspi == NULL) {
		return NULL;
	}

	hDLL = lpaspi->hWnaspiDll = LoadLibrary("WNASPI32.DLL");
	
	if (lpaspi->hWnaspiDll == NULL) {
		free(lpaspi);
		return NULL;
	}

	lpaspi->lpGetASPI32SupportInfo = (DWORD (*)(void))GetProcAddress(hDLL, 
		"GetASPI32SupportInfo");
	lpaspi->lpSendASPI32Command = (DWORD (*)(LPSRB))GetProcAddress(hDLL, 
		"SendASPI32Command");
	lpaspi->lpGetASPI32Buffer = (BOOL (*)(PASPI32BUFF))GetProcAddress(hDLL, 
		"GetASPI32Buffer");
	lpaspi->lpFreeASPI32Buffer = (BOOL (*)(PASPI32BUFF))GetProcAddress(hDLL, 
		"FreeASPI32Buffer");
	lpaspi->lpTranslateASPI32Address = (BOOL (*)(PDWORD, PDWORD))GetProcAddress(hDLL, 
		"TranslateASPI32Address");		
	
	lpaspi->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	dwGetASPI = lpaspi->lpGetASPI32SupportInfo();
	if (HIBYTE(LOWORD(dwGetASPI)) == SS_COMP) {
		lpaspi->nDevice = LOBYTE(LOWORD(dwGetASPI));
	} else {
		/* lpaspi->nDevice = -1; */
		CloseHandle(lpaspi->hEvent);
		FreeLibrary(hDLL);
		free(lpaspi);
		return NULL;
	}

	return lpaspi;
}

void aspi_free(struct configuration *cnf, void *data)
{
	LPASPIDATA lpaspi = (LPASPIDATA)data;
	CloseHandle(lpaspi->hEvent);
	FreeLibrary(lpaspi->hWnaspiDll);
	free(lpaspi);
}


#define	MAX_SCSI_DEVS			16
#define MAX_SCSI_HOSTADAPTERS	64

int aspi_InquiryBuses(void *data, HWND hCtrl, HWND hDlg)
{
	LPASPIDATA lpaspi = (LPASPIDATA)data;
	TVINSERTSTRUCT tvi;
	HTREEITEM hHaIDItem, last;
	CHAR buff[BUFF_SIZE];
	DWORD dwStatus;	
	int HaId, Target, len;
	int LUN = 0, MaxHaId = MAX_SCSI_HOSTADAPTERS;

	SRB32_HAInquiry haInq;
	SRB32_GetSetTimeouts srbTimeOuts;
	SRB32_BusDeviceReset srbReset;
	SRB32_ExecSCSICmd srbExec;
	CHAR str[BUFF_SIZE];
	int nDevice = 0;
	SRB32_RescanPort srbRescan;

	for (HaId = 0; HaId < MaxHaId; HaId++) {
		memset(&haInq, 0, sizeof(SRB32_HAInquiry));
		haInq.SRB_Cmd = SC_HA_INQUIRY;
		haInq.SRB_HaId = HaId;
		haInq.SRB_Flags = 0;

		srbRescan.SRB_Cmd = SC_RESCAN_SCSI_BUS;
		srbRescan.SRB_Flags = 0;
		srbRescan.SRB_HaId = HaId;
		srbRescan.SRB_Status = 0;
		lpaspi->lpSendASPI32Command((LPSRB) &srbRescan);


		dwStatus = lpaspi->lpSendASPI32Command((LPSRB) &haInq);
		if ((haInq.SRB_Status != SS_COMP) || (dwStatus != SS_COMP)) {
			continue;
		}

		MaxHaId = haInq.HA_Count;

		_snprintf(buff, BUFF_SIZE, "HaID:%d - %s (%s)", HaId,
				haInq.HA_Identifier, haInq.HA_ManagerId);

		tvi.hParent = TVI_ROOT;
		tvi.hInsertAfter = TVI_ROOT;
		tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
		tvi.item.pszText = buff;
		tvi.item.lParam = (LUN << 24) + (HaId << 16) + 0xffff;

		hHaIDItem = TreeView_InsertItem(hCtrl, &tvi);
		last = TVI_FIRST;

		for (Target = 0; Target < MAX_SCSI_HOSTADAPTERS; Target++) {
			/* Set timeout to 1 sec */
			srbTimeOuts.SRB_Cmd = SC_GETSET_TIMEOUTS;
			srbTimeOuts.SRB_HaId = HaId;
			srbTimeOuts.SRB_Flags = SRB_DIR_OUT;
			srbTimeOuts.SRB_Target = Target;
			srbTimeOuts.SRB_Lun = 0;
			srbTimeOuts.SRB_Status = 0;
			srbTimeOuts.SRB_Timeout = 5;			
			lpaspi->lpSendASPI32Command(&srbTimeOuts);

			/* Resetting device */
			srbReset.SRB_Cmd = SC_RESET_DEV;
			srbReset.SRB_HaId = HaId;
			srbReset.SRB_Target = Target;
			srbReset.SRB_Lun = 0;
			srbReset.SRB_Status = 0;
			lpaspi->lpSendASPI32Command((LPSRB)&srbReset);

			/* Inquiry device */
			memset(&srbExec, 0, sizeof(SRB32_ExecSCSICmd));
			srbExec.SRB_Cmd = SC_EXEC_SCSI_CMD;
			srbExec.SRB_Flags = SRB_DIR_IN | SRB_EVENT_NOTIFY;
			srbExec.SRB_HaId = HaId;
			srbExec.SRB_Target = Target;
			/* srbExec.SRB_Lun = 0; */
			srbExec.SRB_BufLen = 36;
			srbExec.SRB_BufPointer = (BYTE*)buff;
			srbExec.SRB_SenseLen = SENSE_LEN;
			srbExec.SRB_CDBLen = 6;
			srbExec.SRB_PostProc = (LPVOID)lpaspi->hEvent;			
			srbExec.CDBByte[0] = 0x12;
			srbExec.CDBByte[4] = 0x24;
			
			dwStatus = lpaspi->lpSendASPI32Command((LPSRB)&srbExec);
			
			if (dwStatus == SS_PENDING) {
				WaitForSingleObject(lpaspi->hEvent, 2000);
			}
			
			if (srbExec.SRB_Status == SS_COMP) {
				/* Founded device */
				len = _snprintf(str, BUFF_SIZE, "%d: %.28s", Target,  &buff[8]);
				/*strncpy(str + len, &buff[8], 24);
				str[len + 24] = 0;
				*/
				tvi.hParent = hHaIDItem;
				tvi.hInsertAfter = last;
				tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
				tvi.item.pszText = str;
				tvi.item.lParam = (LUN << 24) + (HaId << 16) + Target; 
				last = TreeView_InsertItem(hCtrl, &tvi);

				TreeView_Expand(hCtrl, hHaIDItem, TVM_EXPAND);

				nDevice++;
			}
		}
	}


	return nDevice;
}


int aspi_CheckSel(void *data, LPARAM lParam) 
{
	if ((lParam & 0xFFFF) == 0xFFFF) {
		return 0;
	}
	return 1;
}

#define THAID	TEXT("HostAdapterID")
#define TLUN	TEXT("LUN")
#define TTARGET	TEXT("Target")

static int apsi_IntToElem(struct configuration *cnf, struct confElement *head, int HaID, int LUN, int Target)
{
	struct confElement *elHaID, *elLUN, *elTarget;

	elLUN = SetParamInt(cnf, head, TLUN, LUN);
	if (elLUN == NULL) {
		return -1;
	}

	elHaID = SetParamInt(cnf, head, THAID, HaID);
	if (elHaID == NULL) {
		removeElement(cnf, elLUN);
		return -1;
	}

	elTarget = SetParamInt(cnf, head, TTARGET, Target);
	if (elTarget == NULL) {
		removeElement(cnf, elHaID);
		removeElement(cnf, elLUN);
		return -1;
	}
	return 3;
}


int apsi_addTarget(void *data, struct configuration *cnf, struct confElement *parent, void *trg)
{
	int HaID = ((long)trg >> 16) & 0xFF;
	int LUN = ((long)trg >> 24) & 0xFF;
	int Target = (long)trg & 0xFF;

	return apsi_IntToElem(cnf, parent, HaID, LUN, Target);
}

static int apsi_ElemToInt(struct confElement *head, int *HaID, int *LUN, int *Target)
{
	struct confElement *elHaID = NULL, *elLUN = NULL, *elTarget = NULL;
	int res;

	if (head->childs) {
		elLUN = findElement(head->childs, TLUN);
		elHaID = findElement(head->childs, THAID);
		elTarget = findElement(head->childs, TTARGET);
	}
	
	if ((elLUN == NULL) || (elHaID == NULL) || (elTarget == NULL)) {
		return -1;
	}

	res = sscanf(elLUN->value, "%d", LUN);
	if (res != 1) {
		return -1;
	}

	res = sscanf(elHaID->value, "%d", HaID);
	if (res != 1) {
		return -1;
	}

	res = sscanf(elTarget->value, "%d", Target);
	if (res != 1) {
		return -1;
	}

	return 0;
}

static int aspi_initPropDlg(HWND hwndDlg, LPDLGPROPDATA dlpd)
{
/*	struct confElement *elHaID = NULL, *elLUN = NULL, *elTarget = NULL; */
	int res;
	int HaID, LUN, Target;

	SetDlgItemText(hwndDlg, IDC_TARGETNAME, dlpd->el->name);
	res = apsi_ElemToInt(dlpd->el, &HaID, &LUN, &Target);
	if (res == -1) {
		return -1;
	}

	SetDlgItemInt(hwndDlg, IDC_LUN, (UINT)LUN, FALSE);
	SetDlgItemInt(hwndDlg, IDC_HAID, (UINT)HaID, FALSE);
	SetDlgItemInt(hwndDlg, IDC_TARGET, (UINT)Target, FALSE);

	return 0;
}

int apsi_CheckDevice(void *data, struct confElement *dev, void *trg, char *inquiryInfo)
{
	SRB32_ExecSCSICmd SRB_Cmd;
	CHAR buff[INQUIRY_LEN];
	DWORD status;
	int HaID, LUN, Target, res;

	memset(&SRB_Cmd, 0, sizeof(SRB32_ExecSCSICmd));
	SRB_Cmd.SRB_Cmd = SC_EXEC_SCSI_CMD;
	SRB_Cmd.SRB_Flags = SRB_EVENT_NOTIFY | SRB_DIR_IN;
	if (dev == NULL) {
		SRB_Cmd.SRB_HaId = ((long)trg >> 16) & 0xFF;
		SRB_Cmd.SRB_Lun = ((long)trg >> 24) & 0xFF;
		SRB_Cmd.SRB_Target = (long)trg & 0xFF;
	} else {
		res = apsi_ElemToInt(dev, &HaID, &LUN, &Target);
		if (res == -1) {
			return -1;
		}
		SRB_Cmd.SRB_HaId = HaID;
		SRB_Cmd.SRB_Lun = LUN;
		SRB_Cmd.SRB_Target = Target;
	}

	SRB_Cmd.SRB_Status = 0;
	SRB_Cmd.SRB_SenseLen = SENSE_LEN;
	SRB_Cmd.SRB_CDBLen = 6;

	SRB_Cmd.SRB_BufLen = INQUIRY_LEN; /*36;*/
	SRB_Cmd.SRB_BufPointer = (BYTE*)buff;
	SRB_Cmd.SRB_PostProc = (LPVOID)((LPASPIDATA)data)->hEvent;
	SRB_Cmd.CDBByte[0] = 0x12;
	SRB_Cmd.CDBByte[4] = INQUIRY_LEN; /*0x24;*/

	status = ((LPASPIDATA)data)->lpSendASPI32Command((LPSRB)&SRB_Cmd);
	if (status == SS_PENDING) {
		WaitForSingleObject(((LPASPIDATA)data)->hEvent, 5000);
	}

	res = (SRB_Cmd.SRB_Status == SS_COMP) ? 0 : -1;
	if ((!res) && (inquiryInfo)) {
		memcpy(inquiryInfo, buff, INQUIRY_LEN);
	}
	return res;
}

INT_PTR CALLBACK aspi_PropDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int res;
	INT id;
	LPDLGPROPDATA dlpd = (LPDLGPROPDATA)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	int HaID, LUN, Target;
	BOOL bHaID, bLUN, bTarget;

	switch (uMsg) {
	case WM_INITDIALOG:
		dlpd = (LPDLGPROPDATA)lParam;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		res = aspi_initPropDlg(hwndDlg, dlpd); 
		if (res == -1) {
			ErrorMessage(hwndDlg, IDS_EBADCONFIG, IDS_TARGETCONF);
			//EndDialog(hwndDlg, IDCANCEL);
		}
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);

		switch (id) {
		case IDOK:
			LUN = GetDlgItemInt(hwndDlg, IDC_LUN, &bLUN, FALSE);
			HaID = GetDlgItemInt(hwndDlg, IDC_HAID, &bHaID, FALSE);
			Target = GetDlgItemInt(hwndDlg, IDC_TARGET, &bTarget, FALSE);
			if ((bLUN) && (bHaID) && (bTarget)) {
				res = apsi_CheckDevice(dlpd->intData, NULL, (void*)((LUN << 24) + (HaID << 16) + Target), NULL);
			} else {
				ErrorMessage(hwndDlg, IDS_ENOTALLPARAM, IDS_TARGETCONF);
				return TRUE;
			}
			if (res == -1) {
				ErrorMessage(hwndDlg, IDS_EDEVICEERROR, IDS_TARGETCONF);
				return TRUE;
			}
			res = apsi_IntToElem(dlpd->cnf, dlpd->el, HaID, LUN, Target);
			if (res == -1) {
				ErrorMessage(hwndDlg, IDS_ECANTCONFIGUER, IDS_TARGETCONF);
				return TRUE;
			}
		case IDCANCEL:
			EndDialog(hwndDlg, id);
			return TRUE;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}

}



/**********************************************************************************/
/* IOCTL probing and functions */


typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH spt;
    ULONG             Filler;      // realign buffers to double word boundary
    UCHAR             ucSenseBuf[32];
    UCHAR             ucDataBuf[512];
} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;

typedef struct tagIOCTLDATA {
	int nDevice;
} IOCTLDATA, *LPIOCTLDATA;

#define DEVICE_LEN 240

typedef struct tagIOCTLTARGETDATA {	
	UCHAR ScsiPortNumber;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;	
} IOCTLTARGETDATA, *LPIOCTLTARGETDATA;

typedef struct tagIOCTLFULLTARGETDATA {	
	IOCTLTARGETDATA td;
	BOOL addHDAInfo;
	char device[DEVICE_LEN];
	BOOL readOnlySRB;
} IOCTLFULLTARGETDATA, *LPIOCTLFULLTARGETDATA;

#undef TLUN

#define TDEVICENAME		TEXT("DeviceFileName")
#define TSCSIPORT		TEXT("SCSIPortNumber")
#define TPATHID			TEXT("PathID")
#define TTARGETID		TEXT("TargetId")
#define TLUN			TEXT("Lun")

#define TREADONLY		TEXT("ReadOnly")

static const char *winDevice[] = {
	"\\\\.\\PhysicalDrive%d",
	"\\\\.\\CdRom%d",
	"\\\\.\\Type%d",
	"\\\\.\\Scaner%d",
	NULL
};

void *ioctl_init(struct configuration *cnf)
{
	DWORD dwVersion = GetVersion();

	if (dwVersion > 0x80000000)  {
		/* Windows 95/98/ME */
		return NULL;
	}
	
	
	return (void*)1;
}

void ioctl_free(struct configuration *cnf, void *data)
{
}

int ioctl_InquiryBuses(void *data, HWND hCtrl, HWND hDlg)
{
	TVINSERTSTRUCT tvi;
	HTREEITEM hDevice, hBus, last;
	CHAR buffer[2048];
	CHAR device[DEVICE_LEN];

	HANDLE hFile;
	DWORD status;	
	int i, j, k, len;
	ULONG returned = 0;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PSCSI_INQUIRY_DATA inquiryData;

	IOCTLTARGETDATA tgData;
	
	for (i = 0; ;i++)	{
		_snprintf(device, DEVICE_LEN, "\\\\.\\Scsi%d:", i);
    
		hFile = CreateFile(device,  GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			break;
		}

		status = DeviceIoControl(hFile, IOCTL_SCSI_RESCAN_BUS, NULL, 0, NULL, 0, &returned, FALSE);


		status = DeviceIoControl(hFile, IOCTL_SCSI_GET_INQUIRY_DATA, NULL, 0, buffer, sizeof(buffer),
                             &returned, FALSE);
	    if (!status) {
			CloseHandle(hFile);
			return -1;
		}

		tvi.hParent = TVI_ROOT;
		tvi.hInsertAfter = TVI_ROOT;
		tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
		tvi.item.pszText = device;
		tvi.item.lParam = 0xffffffff;

		hDevice = TreeView_InsertItem(hCtrl, &tvi);
		hBus = TVI_FIRST;
		tgData.ScsiPortNumber = i;

		adapterInfo = (PSCSI_ADAPTER_BUS_INFO) buffer;

		for (j = 0; j < adapterInfo->NumberOfBuses; j++) {
			_snprintf(device, DEVICE_LEN, "BusID: %d", adapterInfo->BusData[j].InitiatorBusId);
			tvi.hParent = hDevice;
			tvi.hInsertAfter = hBus;
			tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.item.pszText = device;
			tvi.item.lParam = 0xffffffff; 
			hBus = TreeView_InsertItem(hCtrl, &tvi);
			last = TVI_FIRST;

			inquiryData = (PSCSI_INQUIRY_DATA) (buffer + adapterInfo->BusData[j].InquiryDataOffset);
			k = 0;
			while (adapterInfo->BusData[j].InquiryDataOffset) {
				len = _snprintf(device, DEVICE_LEN, "Trg:%d PID:%d LUN:%d -- %.28s", inquiryData->TargetId, 
					inquiryData->PathId, inquiryData->Lun, &inquiryData->InquiryData[8]);
				
				tgData.Lun = inquiryData->Lun;
				tgData.PathId = inquiryData->PathId;
				tgData.TargetId = inquiryData->TargetId;
				/*
				if (inquiryData->DeviceClaimed) {
					_snprintf(device + len, DEVICE_LEN - len, " (claimed)");
				}
				*/
				tvi.hParent = hBus;
				tvi.hInsertAfter = last;
				tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
				tvi.item.pszText = device;
				tvi.item.lParam = *(LPARAM*)&tgData; 
				last = TreeView_InsertItem(hCtrl, &tvi);				
				
				if (inquiryData->NextInquiryDataOffset == 0) {
					break;
				}
				inquiryData = (PSCSI_INQUIRY_DATA) (buffer + inquiryData->NextInquiryDataOffset);
				k++;
			}
			TreeView_Expand(hCtrl, hBus, TVM_EXPAND);

		}
		TreeView_Expand(hCtrl, hDevice, TVM_EXPAND);

		CloseHandle(hFile);
	}


	return 0;
}

int ioctl_CheckSel(void *data, LPARAM lParam) 
{
	if (lParam == 0xffffffff) {
		return 0;
	}
	return 1;
}


static int ioctl_fillTargetData(struct configuration *cnf, struct confElement *parent, 
								LPIOCTLFULLTARGETDATA fd)
{
	struct confElement *scsiPort, *pathId, *lun, *target, *deviceName, *readOnlySRB;

	deviceName = SetParamText(cnf, parent, TDEVICENAME, fd->device);
	if (deviceName == NULL) {
		return -1;
	}

	scsiPort = SetParamInt(cnf, parent, TSCSIPORT, fd->td.ScsiPortNumber);
	if (scsiPort == NULL) {
		removeElement(cnf, deviceName);
		return -1;
	}

	pathId = SetParamInt(cnf, parent, TPATHID, fd->td.PathId);
	if (pathId == NULL) {
		removeElement(cnf, scsiPort);
		removeElement(cnf, deviceName);
		return -1;
	}

	lun = SetParamInt(cnf, parent, TTARGETID, fd->td.TargetId);
	if (lun == NULL) {
		removeElement(cnf, pathId);
		removeElement(cnf, scsiPort);
		removeElement(cnf, deviceName);
		return -1;
	}

	target = SetParamInt(cnf, parent, TLUN, fd->td.Lun);
	if (target == NULL) {
		removeElement(cnf, lun);
		removeElement(cnf, pathId);
		removeElement(cnf, scsiPort);
		removeElement(cnf, deviceName);
		return -1;
	}

	readOnlySRB = SetParamInt(cnf, parent, TREADONLY, fd->readOnlySRB);
	if (readOnlySRB == NULL) {
		removeElement(cnf, target);
		removeElement(cnf, lun);
		removeElement(cnf, pathId);
		removeElement(cnf, scsiPort);
		removeElement(cnf, deviceName);
		return -1;
	}


	return 0;

}

static int ioctl_nativeCheckDevice(LPIOCTLFULLTARGETDATA fd, char *inquiryInfo)
{
	HANDLE hFile;
	SCSI_PASS_THROUGH_WITH_BUFFERS sptdwb;
	UINT length;
	DWORD status, returned;

	hFile = CreateFile(fd->device,  GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE ,
								NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return -1;
	}

	memset(&sptdwb, 0, sizeof(sptdwb));
    sptdwb.spt.Length = sizeof(SCSI_PASS_THROUGH);
    sptdwb.spt.PathId = fd->td.PathId;
    sptdwb.spt.TargetId = fd->td.TargetId;
    sptdwb.spt.Lun = fd->td.Lun;
    sptdwb.spt.CdbLength = 6;
    sptdwb.spt.SenseInfoLength = 24;
    sptdwb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sptdwb.spt.DataTransferLength = INQUIRY_LEN; /*0x24;*/
    sptdwb.spt.TimeOutValue = 2;
	sptdwb.spt.DataBufferOffset = 
		offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
    sptdwb.spt.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptdwb.spt.Cdb[0] = 0x12;
    sptdwb.spt.Cdb[4] = 0x24;
    length = sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS);
    status = DeviceIoControl(hFile, IOCTL_SCSI_PASS_THROUGH,
                             &sptdwb, length, &sptdwb, length, &returned, FALSE);

	if (!status ) {
		return -1;
	} else if (inquiryInfo) {
		memcpy(inquiryInfo, sptdwb.ucDataBuf, INQUIRY_LEN);
	}

	return 0;

}

#define ENODEVICEFOUND	-2

static int ioctl_checkNamedDevice(LPIOCTLFULLTARGETDATA data)
{
	SCSI_ADDRESS sa;
	DWORD status, returned;
	HANDLE hFile = CreateFile(data->device,  GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_ACCESS_DENIED) {
			return ENODEVICEFOUND;
		} else {
			return -1;
		}
	}
	
	status = DeviceIoControl(hFile, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &sa, sizeof(sa),
                             &returned, FALSE);
	
	CloseHandle(hFile);
	if ((!status) || (ioctl_nativeCheckDevice(data, NULL) == -1)) {
		return -1;
	}

	data->td.Lun = sa.Lun;
	data->td.PathId = sa.PathId;
	data->td.ScsiPortNumber = sa.PortNumber;
	data->td.TargetId = sa.TargetId;
	return 0;
}

int ioctl_CheckDevice(void *data, struct confElement *dev, void *trg, char *inquiryInfo)
{
	IOCTLFULLTARGETDATA fd;
	int value;
	struct confElement *el;

	fd.td = *(LPIOCTLTARGETDATA)&trg;
	
	if (dev) {
		el = GetParamInt(dev, TSCSIPORT, &value);
		fd.td.ScsiPortNumber = (el) ? value : 0;

		el = GetParamInt(dev, TPATHID, &value);
		fd.td.PathId = (el) ? value : 0;

		el = GetParamInt(dev, TLUN, &value);
		fd.td.Lun = (el) ? value : 0;

		el = GetParamInt(dev, TTARGETID, &value);
		fd.td.TargetId = (el) ? value : 0;

		if (dev->childs) {
			el = findElement(dev->childs, TDEVICENAME);
		} else {
			el = NULL;
		}
		if (el) {
			strncpy(fd.device, el->value, DEVICE_LEN);
		} else {
			_snprintf(fd.device, DEVICE_LEN, "\\\\.\\Scsi%d:", fd.td.ScsiPortNumber);
		}

	} else {
		_snprintf(fd.device, DEVICE_LEN, "\\\\.\\Scsi%d:", fd.td.ScsiPortNumber);
	}

	return ioctl_nativeCheckDevice(&fd, inquiryInfo);
}

#define MAX_CLASS_DEVICE_NUM	64

int ioctl_addTarget(void *data, struct configuration *cnf, struct confElement *parent, void *trg)
{
	IOCTLFULLTARGETDATA fd;
	int res;
	const char **cls;
	int curNum;
	IOCTLTARGETDATA td;

	/* For first time testing in scsiport mode, this true for unclaimed device */
	fd.td = *(LPIOCTLTARGETDATA)&trg;
	_snprintf(fd.device, DEVICE_LEN, "\\\\.\\Scsi%d:", fd.td.ScsiPortNumber);		
	
	res = ioctl_nativeCheckDevice(&fd, NULL);
	if (res != -1) {
		fd.addHDAInfo = TRUE;
		return ioctl_fillTargetData(cnf, parent, &fd);
	}

	/* Checking for standart windows SCSI class-devices */
	cls = winDevice;
	td = *(LPIOCTLTARGETDATA)&trg;
	while (*cls) {
		for (curNum = 0; curNum < MAX_CLASS_DEVICE_NUM; curNum++) {
			_snprintf(fd.device, DEVICE_LEN, *cls, curNum);
			
			res = ioctl_checkNamedDevice(&fd);
			if (res == ENODEVICEFOUND) {
				break;
			}

			if (res < 0) {
				continue;
			}

			/* Check enumerated device and probed */
			if ((td.Lun == fd.td.Lun) && (td.PathId == fd.td.PathId) &&
				(td.ScsiPortNumber == fd.td.ScsiPortNumber) && (td.TargetId == fd.td.TargetId)) {
				
				fd.addHDAInfo = FALSE; /* for class devices information about HBA not needed */
				return ioctl_fillTargetData(cnf, parent, &fd);
			}

		}
		cls++;
	}
	return -1;
}


static int ioctl_initPropDlg(HWND hwndDlg, LPDLGPROPDATA dlpd)
{
	struct confElement *scsiPort, *pathId, *lun, *target, *deviceName, *readOnlySRB;
	int value;

	SetDlgItemText(hwndDlg, IDC_TARGETNAME, dlpd->el->name);

	if (dlpd->el->childs) {
		deviceName = findElement(dlpd->el->childs, TDEVICENAME);
	} else {
		deviceName = NULL;
	}
	if (deviceName) {
		SetDlgItemText(hwndDlg, IDC_DEVICE, deviceName->value);
	}

	scsiPort = GetParamInt(dlpd->el, TSCSIPORT, &value);
	if (scsiPort) {
		SetDlgItemInt(hwndDlg, IDC_PORTNUMBER, (UINT)value, FALSE);
	}

	pathId = GetParamInt(dlpd->el, TPATHID, &value);
	if (pathId) {
		SetDlgItemInt(hwndDlg, IDC_PATHID, (UINT)value, FALSE);
	}

	lun = GetParamInt(dlpd->el, TLUN, &value);
	if (lun) {
		SetDlgItemInt(hwndDlg, IDC_LUN, (UINT)value, FALSE);
	}

	target = GetParamInt(dlpd->el, TTARGETID, &value);
	if (target) {
		SetDlgItemInt(hwndDlg, IDC_TARGET, (UINT)value, FALSE);
	}

	readOnlySRB = GetParamInt(dlpd->el, TREADONLY, &value);
	if (readOnlySRB) {
		CheckBox_SetCheck(GetDlgItem(hwndDlg, IDC_READONLY), value);
	}
	

	return 0;
}

int ioctl_PropDlgOnOk(HWND hwndDlg, LPDLGPROPDATA dlpd)
{
	UINT scsiPort, pathId, lun, target;
	BOOL bScsiPort, bPathId, bLun, bTarget;
	IOCTLFULLTARGETDATA fd;

	//BOOL readOnlySRB;

	scsiPort = GetDlgItemInt(hwndDlg, IDC_PORTNUMBER, &bScsiPort , FALSE);
	pathId = GetDlgItemInt(hwndDlg, IDC_PATHID, &bPathId , FALSE);
	lun = GetDlgItemInt(hwndDlg, IDC_LUN, &bLun , FALSE);
	target = GetDlgItemInt(hwndDlg, IDC_TARGET, &bTarget , FALSE);

	fd.readOnlySRB = CheckBox_GetCheck(GetDlgItem(hwndDlg, IDC_READONLY));

	fd.td.Lun = (bLun) ? lun : 0;
	fd.td.PathId = (bPathId) ? pathId : 0;
	fd.td.ScsiPortNumber = (bScsiPort) ? scsiPort : 0;
	fd.td.TargetId = (bTarget) ? target : 0;

	GetDlgItemText(hwndDlg, IDC_DEVICE, fd.device, DEVICE_LEN);

	ioctl_fillTargetData(dlpd->cnf, dlpd->el, &fd);
	return ioctl_nativeCheckDevice(&fd, NULL);
}

INT_PTR CALLBACK ioctl_PropDlg(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int res;
	INT id;
	LPDLGPROPDATA dlpd = (LPDLGPROPDATA)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (uMsg) {
	case WM_INITDIALOG:
		dlpd = (LPDLGPROPDATA)lParam;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		
		res = ioctl_initPropDlg(hwndDlg, dlpd); 
		if (res == -1) {
			ErrorMessage(hwndDlg, IDS_EBADCONFIG, IDS_TARGETCONF);
		}
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);

		switch (id) {
		case IDOK:
			res = ioctl_PropDlgOnOk(hwndDlg, dlpd);
			if (res == -1) {
				ErrorMessage(hwndDlg, IDS_EDEVICEERROR, IDS_TARGETCONF);
				return TRUE;
			}
		case IDCANCEL:
			EndDialog(hwndDlg, id);
			return TRUE;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}

}


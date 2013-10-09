#ifndef __EX__WIN32
#define __EX__WIN32

#define SetWindowFont(x, y, z)  SendMessage((x), WM_SETFONT, (WPARAM)(y), (LPARAM)(z))
#define SetWindowIcon(x, y, z)  SendMessage((x), WM_SETICON, (WPARAM)(y), (LPARAM)(z))

#define Button_GetState(x)		SendMessage((x), BM_GETSTATE, 0, 0)
#define Button_SetState(x, y)	SendMessage((x), BM_SETSTATE, (WPARAM)y, 0)
#define Button_GetCheck(x)		SendMessage((x), BM_GETCHECK, 0, 0)
#define Button_SetCheck(x, y)	SendMessage((x), BM_SETCHECK, (WPARAM)y, 0)
#define Button_IsCheck(x)		(BST_CHECKED == SendMessage((x), BM_GETCHECK, 0, 0)) ? TRUE : FALSE
#define Button_IsPushed(x)		(BST_PUSHED == SendMessage((x), BM_GETSTATE, 0, 0)) ? TRUE : FALSE

/* ListBox */
#define ListBox_AddStringW(x, str)			(INT)SendMessageW((x), LB_ADDSTRING, NULL, (LPARAM)(str))
#define ListBox_DeleteStringW(x, pos)		(INT)SendMessageW((x), LB_DELETESTRING, (WPARAM)(pos), NULL)
#define ListBox_InsertStringW(x, str, pos)	(INT)SendMessageW((x), LB_INSERTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ListBox_SelectStringW(x, str, pos)	(INT)SendMessageW((x), LB_SELECTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ListBox_GetTextW(x, pos, str)		(INT)SendMessageW((x), LB_GETTEXT, (WPARAM)pos, (LPARAM)str)	

#define ListBox_AddStringA(x, str)			(INT)SendMessageA((x), LB_ADDSTRING, NULL, (LPARAM)(str))
#define ListBox_DeleteStringA(x, pos)		(INT)SendMessageA((x), LB_DELETESTRING, (WPARAM)(pos), NULL)
#define ListBox_InsertStringA(x, str, pos)	(INT)SendMessageA((x), LB_INSERTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ListBox_SelectStringA(x, str, pos)	(INT)SendMessageA((x), LB_SELECTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ListBox_GetTextA(x, pos, str)		(INT)SendMessageA((x), LB_GETTEXT, (WPARAM)pos, (LPARAM)str)	

#ifdef UNICODE
#define ListBox_AddString					ListBox_AddStringW
#define ListBox_DeleteString				ListBox_DeleteStringW
#define ListBox_InsertString				ListBox_InsertStringW
#define ListBox_SelectString				ListBox_SelectStringW
#define ListBox_GetText						ListBox_GetTextW
#else
#define ListBox_AddString					ListBox_AddStringA
#define ListBox_DeleteString				ListBox_DeleteStringA
#define ListBox_InsertString				ListBox_InsertStringA
#define ListBox_SelectString				ListBox_SelectStringA
#define ListBox_GetText						ListBox_GetTextA

#endif

#define ListBox_GetItemData(x, index)		SendMessage((x), LB_GETITEMDATA, (WPARAM)(index), 0)
#define ListBox_SetItemData(x, index, val)	SendMessage((x), LB_SETITEMDATA, (WPARAM)(index), (LPARAM)(val))
#define ListBox_GetCount(x)					(INT)SendMessage((x), LB_GETCOUNT, 0, 0)
#define ListBox_GetCurSel(x)				(INT)SendMessage((x), LB_GETCURSEL, 0, 0)
#define ListBox_SetCurSel(x, index)			(INT)SendMessage((x), LB_SETCURSEL, (WPARAM)index, 0)
#define ListBox_GetTextLen(x, pos)			(INT)SendMessage((x), LB_GETTEXTLEN, (WPARAM)pos, 0)

/* ComboBox */
#define ComboBox_AddStringW(x, str)			(INT)SendMessageW((x), CB_ADDSTRING, 0, (LPARAM)(str))
#define ComboBox_DeleteStringW(x, pos)		(INT)SendMessageW((x), CB_DELETESTRING, (WPARAM)(pos), 0)
#define ComboBox_InsertStringW(x, str, pos)	(INT)SendMessageW((x), CB_INSERTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ComboBox_SelectStringW(x, str, pos)	(INT)SendMessageW((x), CB_SELECTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ComboBox_GetTextW(x, pos, str)		(INT)SendMessageW((x), CB_GETTEXT, (WPARAM)pos, (LPARAM)str)	

#define ComboBox_AddStringA(x, str)			(INT)SendMessageA((x), CB_ADDSTRING, 0, (LPARAM)(str))
#define ComboBox_DeleteStringA(x, pos)		(INT)SendMessageA((x), CB_DELETESTRING, (WPARAM)(pos), 0)
#define ComboBox_InsertStringA(x, str, pos)	(INT)SendMessageA((x), CB_INSERTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ComboBox_SelectStringA(x, str, pos)	(INT)SendMessageA((x), CB_SELECTSTRING, (WPARAM)(pos), (LPARAM)(str))
#define ComboBox_GetTextA(x, pos, str)		(INT)SendMessageA((x), CB_GETTEXT, (WPARAM)pos, (LPARAM)str)	

#ifdef UNICODE
#define ComboBox_AddString					ComboBox_AddStringW
#define ComboBox_DeleteString				ComboBox_DeleteStringW
#define ComboBox_InsertString				ComboBox_InsertStringW
#define ComboBox_SelectString				ComboBox_SelectStringW
#define ComboBox_GetText					ComboBox_GetTextW
#else
#define ComboBox_AddString					ComboBox_AddStringA
#define ComboBox_DeleteString				ComboBox_DeleteStringA
#define ComboBox_InsertString				ComboBox_InsertStringA
#define ComboBox_SelectString				ComboBox_SelectStringA
#define ComboBox_GetText					ComboBox_GetTextA
#endif

#define ComboBox_GetItemData(x, index)		SendMessage((x), CB_GETITEMDATA, (WPARAM)(index), 0)
#define ComboBox_SetItemData(x, index, val)	SendMessage((x), CB_SETITEMDATA, (WPARAM)(index), (LPARAM)(val))
#define ComboBox_GetCount(x)				(INT)SendMessage((x), CB_GETCOUNT, 0, 0)
#define ComboBox_GetCurSel(x)				(INT)SendMessage((x), CB_GETCURSEL, 0, 0)
#define ComboBox_SetCurSel(x, index)		(INT)SendMessage((x), CB_SETCURSEL, (WPARAM)index, 0)
#define ComboBox_GetTextLen(x, pos)			(INT)SendMessage((x), CB_GETTEXTLEN, (WPARAM)pos, 0)


#endif

// ITaskMgr.cpp
//

#include "stdafx.h"
#include "ITaskMgr.h"

INT_PTR CALLBACK DlgProcCpu(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcProcess(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcTask(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcInfo(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

#ifdef _WIN32_WCE
#define MAIN_DLG_CX 315
#else
#define MAIN_DLG_CX 420
#endif

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK DlgProcHelp(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

static BOOL CreateTab(ThreadPack* pTP);
static BOOL CreateIcon(ThreadPack* pTP);
static BOOL CreateThreads(ThreadPack* pTP);

static void Measure(ThreadPack* pTP);
static DWORD CALLBACK thIdle(LPVOID pvParams);
static DWORD GetThreadTick(FILETIME* a, FILETIME* b);

HINSTANCE	g_hInst;
BOOL volatile g_bThreadEnd;

//-----------------------------------------------------------------------------
// WinMain entry point
//-----------------------------------------------------------------------------
int WINAPI _tWinMain(	HINSTANCE hInstance,
						HINSTANCE hPrevInstance,
						LPTSTR    lpCmdLine,
						int       nCmdShow)
{
	INITCOMMONCONTROLSEX icex;

	// Ensure that the common control DLL is loaded. 
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC  = ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex); 

	g_hInst = hInstance;

	//------------ Prevent multiple instance  ------------------
	HANDLE hMutex = CreateMutex(NULL,FALSE,APPNAME);

	if( GetLastError() == ERROR_ALREADY_EXISTS )
	{
		if (HWND hwndPrev = FindWindow(WC_DIALOG, APPNAME))
		{
			ShowWindow(hwndPrev, SW_SHOWNORMAL);
			SetForegroundWindow(hwndPrev);
		}
		return 0; 
	}


	// g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINDLG), NULL, DlgProc);
    return 0;
}
static struct shellapi {
	HMODULE h;
	DllImport<BOOL(WINAPI*)(DWORD, NOTIFYICONDATA*)> Shell_NotifyIcon;
} const shellapi = {
#ifdef _WIN32_WCE
	GetModuleHandle(_T("COREDLL.DLL")),
#else
	LoadLibrary(_T("SHELL32.DLL")),
#endif
	GetProcAddressA(shellapi.h, _CRT_STRINGIZE(Shell_NotifyIcon)),
};

//-----------------------------------------------------------------------------
// main dialog
//-----------------------------------------------------------------------------
static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;
	LPNMHDR lpnmhdr;
	
	switch(Msg)
	{
	// ----------------------------------------------------------
	case WM_INITDIALOG:
	{
		if( pTP == NULL )
		pTP = (ThreadPack*)LocalAlloc(LPTR, sizeof(ThreadPack));
		if( pTP == NULL )
		{
			EndDialog(hDlg, 0);
			return FALSE;
		}
		memset(pTP, 0, sizeof(ThreadPack));

		pTP->hDlg = hDlg;
		g_bThreadEnd/*pTP->bEnd*/ = FALSE;
		pTP->dwInterval = 2000; //sec
		pTP->g_hInst = g_hInst;

		memset(pTP->chPowHistory, -1, sizeof pTP->chPowHistory);
		memset(pTP->chPowHistory, 0, sizeof *pTP->chPowHistory);
		memset(pTP->chMemHistory, -1, sizeof pTP->chMemHistory);
		memset(pTP->chMemHistory, 0, sizeof *pTP->chMemHistory);

		if (CreateThreads(pTP) == FALSE
			|| CreateTab(pTP) == FALSE
			|| CreateIcon(pTP) == FALSE)
		{
			MessageBox(hDlg, _T("Application initialize failed."), APPNAME, MB_ICONERROR);
			EndDialog(hDlg, 0);
			return FALSE;
		}

		pTP->nMode = MODE_CPUPOWER;

		
		// make tasktray icons
		pTP->nidTrayIcon.cbSize				= sizeof(NOTIFYICONDATA);
		pTP->nidTrayIcon.hIcon				= pTP->hIcon[0];
		pTP->nidTrayIcon.hWnd				= hDlg;
		pTP->nidTrayIcon.uCallbackMessage	= MY_NOTIFYICON;
		pTP->nidTrayIcon.uFlags				= NIF_MESSAGE | NIF_ICON;
		pTP->nidTrayIcon.uID				= 1;

		if (*shellapi.Shell_NotifyIcon)
			(*shellapi.Shell_NotifyIcon)(NIM_ADD, &pTP->nidTrayIcon);

		SetTimer(hDlg, 1, pTP->dwInterval, NULL);

		RECT rcWorkArea;
		if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, FALSE))
		{
			SetWindowPos(hDlg, NULL
				, (rcWorkArea.right / 2) - (MAIN_DLG_CX / 2)
				, 0
				, MAIN_DLG_CX
				, (rcWorkArea.bottom > 240) ? 240 : rcWorkArea.bottom
				, SWP_NOZORDER);
		}

		return TRUE;
	}

	// ----------------------------------------------------------
	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_STAY_ON_TOP:
			SetWindowPos(hDlg, IsDlgButtonChecked(hDlg, IDC_STAY_ON_TOP) ?
				HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		return TRUE;

	// ----------------------------------------------------------
	case WM_NOTIFY: 

		lpnmhdr = (LPNMHDR)lParam;

		if( (lpnmhdr->hwndFrom == pTP->hwndTab)
			&& (lpnmhdr->idFrom == IDC_TAB)
			&& (lpnmhdr->code == TCN_SELCHANGE))
		{
			pTP->nMode = TabCtrl_GetCurSel(pTP->hwndTab);
			SetWindowPos(hDlg, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		break; 

	// ----------------------------------------------------------
	case WM_WINDOWPOSCHANGED:
	{
		LPWINDOWPOS lpwndpos = (LPWINDOWPOS)lParam;

		RECT rc;
		GetClientRect(hDlg, &rc);

		// Size the tab control to fit the client area.
		HDWP hdwp = BeginDeferWindowPos(6);

		UINT flags = lpwndpos->flags & SWP_NOSIZE ? SWP_NOMOVE | SWP_NOSIZE : 0;
		if (flags == 0)
		{
			RECT rcTab;
			GetWindowRect(pTP->hwndTab, &rcTab);
			MapWindowPoints(pTP->hwndStayOnTop, hDlg, (LPPOINT)&rcTab, 1);
			DeferWindowPos(hdwp, pTP->hwndStayOnTop, NULL,
				rcTab.left + rc.right - rcTab.right, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			DeferWindowPos(hdwp, pTP->hwndTab, NULL,
				rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
			TabCtrl_AdjustRect(pTP->hwndTab, FALSE, &rc);
		}

		DeferWindowPos(hdwp, pTP->hwndCpupower, HWND_TOP,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
			flags | (pTP->nMode == MODE_CPUPOWER ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));

		DeferWindowPos(hdwp, pTP->hwndProcessList, HWND_TOP,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
			flags | (pTP->nMode == MODE_PROCESS ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));

		DeferWindowPos(hdwp, pTP->hwndTaskList, HWND_TOP,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
			flags | (pTP->nMode == MODE_TASKLIST ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
		
		DeferWindowPos(hdwp, pTP->hwndInfo, HWND_TOP,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
			flags | (pTP->nMode == MODE_INFO ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));

		EndDeferWindowPos(hdwp);
	}
	break; 

	// ----------------------------------------------------------
	case MY_NOTIFYICON:
		switch(lParam)
		{
		case WM_LBUTTONDOWN:
		{
			if(IsWindowVisible(hDlg) )
			{
				ShowWindow(hDlg, SW_HIDE);
			}
			else
			{
				ShowWindow(hDlg, SW_SHOWNORMAL);
				SetForegroundWindow(hDlg);
			}

			break;
		}
		default:
			break;
		}
		break;
	// ----------------------------------------------------------
#ifdef _WIN32_WCE
	case WM_HELP:
		DialogBox(pTP->g_hInst, MAKEINTRESOURCE(IDD_HELP), hDlg, DlgProcHelp);
		break;
#else
	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_CONTEXTHELP:
			DialogBox(pTP->g_hInst, MAKEINTRESOURCE(IDD_HELP), hDlg, DlgProcHelp);
			return TRUE;
		}
		break;
#endif

	// ----------------------------------------------------------
	case WM_TIMER:
		Measure(pTP);

		if(!IsWindowVisible(hDlg))
			return 0;

		switch(pTP->nMode)
		{
		case MODE_ICON:
			break;
		case MODE_CPUPOWER:
			PostMessage(pTP->hwndCpupower, WM_TIMER, wParam, lParam);
			break;
		case MODE_PROCESS:
			PostMessage(pTP->hwndProcessList, WM_TIMER, wParam, lParam);
			break;
		case MODE_TASKLIST:
			PostMessage(pTP->hwndTaskList, WM_TIMER, wParam, lParam);
			break;
		default:
			break;
		}
		break;

	// ----------------------------------------------------------
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;

	// ----------------------------------------------------------
	case WM_DESTROY:
		KillTimer(hDlg, 1);
		if (*shellapi.Shell_NotifyIcon)
			(*shellapi.Shell_NotifyIcon)(NIM_DELETE, &pTP->nidTrayIcon);

		if (pTP)
		{
			g_bThreadEnd = TRUE;
			for (DWORD i = 0; i < pTP->si.dwNumberOfProcessors; ++i)
			{
				ResumeThread(pTP->hIdleThread[i]);
				WaitForSingleObject(pTP->hIdleThread[i], 3000);
			}
			LocalFree(pTP);
		}
		break;
	}
	
	return FALSE;
}

//-----------------------------------------------------------------------------
// create tab control
//-----------------------------------------------------------------------------
static BOOL CreateTab(ThreadPack* pTP)
{
	HWND hwndTab = GetDlgItem(pTP->hDlg, IDC_TAB);
	pTP->hwndTab = hwndTab;

	if( hwndTab == NULL )
		return FALSE;

	pTP->hwndStayOnTop = GetDlgItem(pTP->hDlg, IDC_STAY_ON_TOP);

	TCITEM tie; 

	tie.mask = TCIF_TEXT | TCIF_IMAGE; 
	tie.iImage = -1; 
	
	// ---------------------------------------------------- CPUPOWER
	tie.pszText = _T("CPU");
	
	if(TabCtrl_InsertItem(hwndTab, MODE_CPUPOWER, &tie) == -1)
		return FALSE;

	HWND const hwndCpupower = CreateDialogParam( pTP->g_hInst, MAKEINTRESOURCE(IDD_CPU), pTP->hDlg, DlgProcCpu, (LPARAM)pTP );

	if( hwndCpupower == NULL )
		return FALSE;
	
	// ---------------------------------------------------- PROCESSLIST
	tie.pszText = _T("Process");

	if(TabCtrl_InsertItem(hwndTab, MODE_PROCESS, &tie) == -1)
		return FALSE;

	HWND const hwndProcessList = CreateDialogParam( pTP->g_hInst, MAKEINTRESOURCE(IDD_PROCESS_LIST), pTP->hDlg, DlgProcProcess, (LPARAM)pTP );

	if( hwndProcessList == NULL )
		return FALSE;
	
	// ---------------------------------------------------- TASKLIST
	tie.pszText = _T("Task");
	if(TabCtrl_InsertItem(hwndTab, MODE_TASKLIST, &tie) == -1)
		return FALSE;

	HWND const hwndTaskList = CreateDialogParam( pTP->g_hInst, MAKEINTRESOURCE(IDD_TASK_LIST), pTP->hDlg, DlgProcTask, (LPARAM)pTP );

	if( hwndTaskList == NULL )
		return FALSE;
	
	// ---------------------------------------------------- INFO
	tie.pszText = _T("Info");
	if (TabCtrl_InsertItem(hwndTab, MODE_INFO, &tie) == -1)
		return FALSE;

	HWND const hwndInfo = CreateDialogParam(pTP->g_hInst, MAKEINTRESOURCE(IDD_SYSTEM_INFO), pTP->hDlg, DlgProcInfo, (LPARAM)pTP);

	if (hwndTaskList == NULL)
		return FALSE;

	// ---------------------------------------------------- ADD

	pTP->hwndProcessList = hwndProcessList;
	pTP->hwndCpupower = hwndCpupower;
	pTP->hwndTaskList = hwndTaskList;
	pTP->hwndInfo = hwndInfo;

	return TRUE;

}

//-----------------------------------------------------------------------------
//	load icon image from resource block
//-----------------------------------------------------------------------------
static BOOL CreateIcon(ThreadPack* pTP)
{
	HICON* pIcon = pTP->hIcon;
	for( int ii = 0; ii < 12; ii++, pIcon++ )
	{
		*pIcon = (HICON)LoadImage( pTP->g_hInst,
				MAKEINTRESOURCE(IDI_CPU_00 + ii), IMAGE_ICON, 16,16,0);
		if( pIcon == NULL )
			return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// measure cpu power and ...
//-----------------------------------------------------------------------------
static void Measure(ThreadPack* pTP)
{
	static DWORD dwLastThreadTime[CPUCORE_MAX] = { 0 };
	static DWORD dwLastTickTime = 0;
	DWORD dwCurrentThreadTime[CPUCORE_MAX] = { 0 };
	DWORD dwCurrentTickTime = GetTickCount();

	FILETIME ftCreationTime;
	FILETIME ftExitTime;
	FILETIME ftKernelTime;
	FILETIME ftUserTime;

	// Shift history
	size_t z = sizeof *pTP->chPowHistory;
	memmove(pTP->chPowHistory + 1, pTP->chPowHistory, sizeof pTP->chPowHistory - sizeof *pTP->chPowHistory);
	memmove(pTP->chMemHistory + 1, pTP->chMemHistory, sizeof pTP->chMemHistory - sizeof *pTP->chMemHistory);

	MEMORYSTATUS ms;
	ms.dwLength = sizeof(MEMORYSTATUS);

	// memory history
	GlobalMemoryStatus(&ms);
	*pTP->chMemHistory = (char)ms.dwMemoryLoad;

	// cpu histroy
	DWORD dwCpuPower = 0;
	for (DWORD i = 0; i < pTP->si.dwNumberOfProcessors; ++i)
	{
		SuspendThread(pTP->hIdleThread[i]);
		GetThreadTimes(pTP->hIdleThread[i], &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUserTime);
		dwCurrentThreadTime[i] = GetThreadTick(&ftKernelTime, &ftUserTime);

		DWORD dwCorePower;
		// calculate cpu power
		if (dwCurrentTickTime != dwLastTickTime && dwLastThreadTime[i] != 0 && dwLastTickTime != 0)
			dwCorePower = 100 - (((dwCurrentThreadTime[i] - dwLastThreadTime[i]) * 100) / (dwCurrentTickTime - dwLastTickTime));
		else
			dwCorePower = 0;	// avoid 0div
		pTP->chPowHistory[0][i] = (char)dwCorePower;
		dwCpuPower += dwCorePower;
		dwLastThreadTime[i] = dwCurrentThreadTime[i];
		ResumeThread(pTP->hIdleThread[i]);
	}
	dwCpuPower /= pTP->si.dwNumberOfProcessors;

	// Draw tray icon
	int nIconIndex = (dwCpuPower)*11/100;
	if( nIconIndex > 11 || nIconIndex < 0 )
		nIconIndex = 0;
	pTP->nidTrayIcon.hIcon = pTP->hIcon[nIconIndex];
	if (*shellapi.Shell_NotifyIcon)
		(*shellapi.Shell_NotifyIcon)(NIM_MODIFY, &pTP->nidTrayIcon);

	// save status
	dwLastTickTime = GetTickCount();
}

//-----------------------------------------------------------------------------
// dummy thread for mesure cpu power
//-----------------------------------------------------------------------------
static DWORD CALLBACK thIdle(LPVOID pvParams)
{
	ThreadPack* pTP = (ThreadPack*)pvParams;

	while(!g_bThreadEnd/*pTP->bEnd*/);

	return 0;
}

//-----------------------------------------------------------------------------
// helper
//-----------------------------------------------------------------------------
static DWORD GetThreadTick(FILETIME* a, FILETIME* b)
{
	__int64 a64 = 0;
	__int64 b64 = 0;
	a64 = a->dwHighDateTime;
	a64 <<= 32;
	a64 += a->dwLowDateTime;

	b64 = b->dwHighDateTime;
	b64 <<= 32;
	a64 += b->dwLowDateTime;

	a64 += b64;

	// nano sec to milli sec
	a64 /= 10000;

	return (DWORD)a64;
}

//-----------------------------------------------------------------------------
// about dlg proc
//-----------------------------------------------------------------------------
static INT_PTR CALLBACK DlgProcHelp(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, wParam);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// HelperFunction
//-----------------------------------------------------------------------------
LPARAM GetSelectedItemLParam(HWND hwndLView)
{
	int nIndex = -1;
	LVITEM lvItem;
	memset(&lvItem, 0, sizeof(LVITEM));
	lvItem.mask = LVIF_PARAM;
	
	nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);

	while( nIndex != -1 )
	{
		lvItem.iItem = nIndex;
		if( ListView_GetItemState(hwndLView, nIndex, LVIS_SELECTED) )
		{
			ListView_GetItem(hwndLView, &lvItem);
			return lvItem.lParam;
		}
		nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// select item by lparam
//-----------------------------------------------------------------------------
void SelectItemLParam(HWND hwndLView, LPARAM lParam)
{
	// serch item
	LVFINDINFO finditem;
	memset(&finditem, 0, sizeof finditem);

	finditem.flags = LVFI_PARAM;
	finditem.lParam = lParam;

	int nIndex = ListView_FindItem(hwndLView, -1, &finditem);

	if (nIndex != -1)
	{
		ListView_SetItemState(hwndLView, nIndex, LVIS_SELECTED, LVIS_SELECTED);
		ListView_EnsureVisible(hwndLView, nIndex, FALSE);
	}
}

//-----------------------------------------------------------------------------
// create thread(s)
//-----------------------------------------------------------------------------
static BOOL CreateThreads(ThreadPack* pTP)
{
	GetSystemInfo(&pTP->si);
	pTP->si.dwNumberOfProcessors;
	for (DWORD i = 0; i < pTP->si.dwNumberOfProcessors; ++i)
	{
		pTP->hIdleThread[i] = CreateThread(NULL, 0, thIdle, pTP, CREATE_SUSPENDED, NULL);
		SetThreadPriority(pTP->hIdleThread[i], THREAD_PRIORITY_IDLE);
		CeSetThreadAffinity(pTP->hIdleThread[i], 1 << i);
		ResumeThread(pTP->hIdleThread[i]);
	}
	return TRUE;
}

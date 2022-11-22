#include "stdafx.h"
#include "ITaskMgr.h"

static SIZE_T CalcHeapOfProcess(DWORD dwProcessID);
static void InitProcessListViewColumns(HWND hwndLView);
static BOOL InsertProcessItem(HWND hwndLView, PROCESSENTRY32* ppe32);
static void KillSelectedProcess(HWND hwndLView);
static BOOL DrawProcessView(HWND hwndLView);
static void ResizeWindow(HWND hDlg, LPARAM lParam);

//-----------------------------------------------------------------------------
// process listview dialog
//-----------------------------------------------------------------------------
INT_PTR CALLBACK DlgProcProcess(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;

	switch(Msg)
	{

	// ----------------------------------------------------------
	case WM_INITDIALOG:
		pTP = (ThreadPack*)lParam;
		if (HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS))
		{
			ListView_SetExtendedListViewStyle(hwndLView, LVS_EX_FULLROWSELECT);
			InitProcessListViewColumns(hwndLView);
		}
		return TRUE;

	// ----------------------------------------------------------
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom == IDC_LV_PROCESS)
		{
			LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lParam;
			switch (lpnmlv->hdr.code)
			{
			case LVN_ITEMCHANGED:
				if ((lpnmlv->uChanged & LVIF_STATE) &&
					(lpnmlv->uNewState & LVIS_SELECTED) > (lpnmlv->uOldState & LVIS_SELECTED))
				{
					pTP->dwSelectedProcessID = (DWORD)lpnmlv->lParam;
				}
				break;
			case NM_DBLCLK:
				TabCtrl_SetCurSel(pTP->hwndTab, MODE_THREAD);
				NMHDR nmh = { pTP->hwndTab, IDC_TAB, TCN_SELCHANGE };
				SendMessage(pTP->hDlg, WM_NOTIFY, 0, (LPARAM)&nmh);
				SetFocus(GetNextDlgTabItem(pTP->hwndThreadList, NULL, FALSE));
				break;
			}
		}
		break;

	// ----------------------------------------------------------
	case WM_COMMAND:
		switch( LOWORD(wParam) )
		{
		case IDC_TERMINATE:
			HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS);
			KillSelectedProcess(hwndLView);
		}
		break;

	// ----------------------------------------------------------
	case WM_SIZE:
		ResizeWindow(hDlg, lParam);
		break;

	// ----------------------------------------------------------
	case WM_WINDOWPOSCHANGED:
		if (((LPWINDOWPOS)lParam)->flags & (SWP_SHOWWINDOW | SWP_FRAMECHANGED))
		{
			if (HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS))
			{
				DrawProcessView(hwndLView);
			}
			if (HWND hwndHeap = GetDlgItem(hDlg, IDC_HEAP))
			{
				TCHAR szFmt[128];
				SIZE_T dwUsedHeap = CalcHeapOfProcess(pTP->dwSelectedProcessID);
				wsprintf(szFmt, _T("Memory used %uKB"), (DWORD)(dwUsedHeap >> 10));
				SetWindowText(hwndHeap, szFmt);
			}
		}
		break;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// draw graph of process and memory
//-----------------------------------------------------------------------------
static BOOL DrawProcessView(HWND hwndLView)
{
	HANDLE const hSS = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSS == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	// -- process

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof pe32;

	LPARAM lParam[PROCESS_MAX];

	int n = 0;

	if (Process32First(hSS, &pe32)) do
	{
		InsertProcessItem(hwndLView, &pe32);
		lParam[n++] = pe32.th32ProcessID;
	} while (Process32Next(hSS, &pe32) && (n < PROCESS_MAX));

	DeleteExcessItemsLParam(hwndLView, lParam, n);

	CloseToolhelp32Snapshot(hSS);

	return TRUE;
}

//-----------------------------------------------------------------------------
// make columns header
//-----------------------------------------------------------------------------
static void InitProcessListViewColumns(HWND hwndLView)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 

	// Process Image
	lvc.iSubItem = 0;
	lvc.pszText = _T("image name");
	lvc.cx = 100;
	lvc.fmt = LVCFMT_LEFT;
	ListView_InsertColumn(hwndLView, 0, &lvc);

	// Process ID
	lvc.iSubItem = 1;
	lvc.pszText = _T("id");
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("0000000000"));
	lvc.fmt = LVCFMT_LEFT;
	ListView_InsertColumn(hwndLView, 1, &lvc);

	// Process Priority
	lvc.iSubItem = 3;
	lvc.pszText = _T("prio");
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("00000"));
	lvc.fmt = LVCFMT_RIGHT;
	ListView_InsertColumn(hwndLView, 2, &lvc);

	// Process Affinity
	lvc.iSubItem = 4;
	lvc.pszText = _T("affin");
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("00000"));
	lvc.fmt = LVCFMT_RIGHT;
	ListView_InsertColumn(hwndLView, 3, &lvc);

	// Process Threads
	lvc.iSubItem = 2;
	lvc.pszText = _T("thrds");
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("000000"));
	lvc.fmt = LVCFMT_RIGHT;
	ListView_InsertColumn(hwndLView, 4, &lvc);
}

//-----------------------------------------------------------------------------
// fill process view
//-----------------------------------------------------------------------------
static BOOL InsertProcessItem(HWND hwndLView, PROCESSENTRY32* ppe32)
{
	// search item
	LVFINDINFO finditem;
	memset(&finditem, 0, sizeof finditem);
	
	finditem.flags = LVFI_PARAM;
	finditem.lParam = ppe32->th32ProcessID;

	DWORD dwIndex = ListView_FindItem(hwndLView, -1, &finditem);

	TCHAR szFmt[256];

	if (dwIndex == -1)
	{
		LVITEM lvItem;
		memset(&lvItem, 0, sizeof(LVITEM));

		lvItem.mask = LVIF_TEXT | LVIF_PARAM;
		lvItem.iItem = 0;
		lvItem.iSubItem = 0;
		lvItem.pszText = ppe32->szExeFile;
		lvItem.lParam = ppe32->th32ProcessID;

		dwIndex = ListView_InsertItem(hwndLView, &lvItem);

		if( dwIndex == -1 )
		{
			return FALSE;
		}

		// Add nonvolatile subitems

		// id
		wsprintf(szFmt, _T("%08X"), ppe32->th32ProcessID);
		ListView_SetItemText( hwndLView, dwIndex, 1, szFmt );
	}

	// Add volatile subitems

	// priority
	wsprintf(szFmt, _T("%d"), ppe32->pcPriClassBase);
	ListView_SetItemText(hwndLView, dwIndex, 2, szFmt);

	// affinity
	DWORD dwAffinity;
	if (CeGetProcessAffinity((HANDLE)ppe32->th32ProcessID, &dwAffinity))
	{
		wsprintf(szFmt, _T("%02X"), dwAffinity);
		ListView_SetItemText(hwndLView, dwIndex, 3, szFmt);
	}

	// threads
	wsprintf(szFmt, _T("%d"), ppe32->cntThreads);
	ListView_SetItemText(hwndLView, dwIndex, 4, szFmt);

	return TRUE;
}

//-----------------------------------------------------------------------------
// kill selected process
//-----------------------------------------------------------------------------
static void KillSelectedProcess(HWND hwndLView)
{
	DWORD dwProcessID = (DWORD)GetSelectedItemLParam(hwndLView);

	if( dwProcessID == 0 )
		return;

	if( IDOK == MessageBox(hwndLView
		, _T("Warning: After ending a process, data is lost or a system becomes unstable.")
		, APPNAME 
		, MB_OKCANCEL|MB_DEFBUTTON2|MB_APPLMODAL|MB_ICONEXCLAMATION ) )
	{
		if (HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessID))
		{
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
	}
}

//-----------------------------------------------------------------------------
// calc process loaded memory
//-----------------------------------------------------------------------------
static SIZE_T CalcHeapOfProcess(DWORD dwProcessID)
{
	SIZE_T dwUsedHeap = 0;
#ifdef _WIN32_WCE
	if( dwProcessID == 0 )
		return 0;

	// -- heap
	HANDLE hSS;
	hSS = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, dwProcessID);

	if( hSS == INVALID_HANDLE_VALUE )
	{
		return FALSE;
	}	

	HEAPLIST32 hl32;
	memset(&hl32, 0, sizeof(HEAPLIST32));
	hl32.dwSize = sizeof(HEAPLIST32);

	if( !Heap32ListFirst(hSS, &hl32) )
	{
		CloseToolhelp32Snapshot(hSS);
		if( GetLastError() == ERROR_NO_MORE_FILES )
		{
			return 0;
		}
		return 0;
	}

	DWORD dwTotalHeapByte = 0;
	do
	{
		HEAPENTRY32 he32;
		memset(&he32, 0, sizeof(HEAPENTRY32));
		he32.dwSize = sizeof(HEAPENTRY32);

		if( !Heap32First( hSS, &he32, hl32.th32ProcessID, hl32.th32HeapID ) )
		{
			CloseToolhelp32Snapshot(hSS);
			return 0;
		}

		do
		{
			if( he32.dwFlags != LF32_FREE )
				dwTotalHeapByte += he32.dwBlockSize;
		}while( Heap32Next( hSS, &he32 ) );
	
		if( GetLastError() != ERROR_NO_MORE_FILES )
		{
			CloseToolhelp32Snapshot(hSS);
			return 0;
		}

		dwUsedHeap += dwTotalHeapByte;

	}while( Heap32ListNext( hSS, &hl32 ) );

	if( GetLastError() != ERROR_NO_MORE_FILES )
	{
		dwUsedHeap = 0;
	}
	CloseToolhelp32Snapshot(hSS);
#else
	if (HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessID))
	{
		PROCESS_MEMORY_COUNTERS pmc;
		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof pmc))
		{
			dwUsedHeap = pmc.WorkingSetSize;
		}
		CloseHandle(hProcess);
	}
#endif
	return dwUsedHeap;
}

//-----------------------------------------------------------------------------
// Resize all window
//-----------------------------------------------------------------------------
static void ResizeWindow(HWND hDlg, LPARAM lParam)
{
	HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS);
	HWND hwndTermBtn = GetDlgItem(hDlg, IDC_TERMINATE);
	HWND hwndHeap = GetDlgItem(hDlg, IDC_HEAP);

	RECT rcTab;
	RECT rcTermBtn;

	SetRect(&rcTab, 0, 0, LOWORD(lParam), HIWORD(lParam));
	TabCtrl_AdjustRect(hDlg, FALSE, &rcTab);

	GetClientRect(hwndTermBtn, &rcTermBtn);

	HDWP hdwp = BeginDeferWindowPos(3);
	
	hdwp = DeferWindowPos(hdwp, hwndLView, NULL
		, rcTab.left
		, rcTab.top
		, rcTab.right - rcTab.left
		, rcTab.bottom - rcTermBtn.bottom
		, SWP_NOZORDER);

	hdwp = DeferWindowPos(hdwp, hwndTermBtn, NULL
		, rcTab.right - rcTermBtn.right
		, rcTab.bottom - rcTermBtn.bottom
		, rcTermBtn.right
		, rcTermBtn.bottom
		, SWP_NOZORDER);

	hdwp = DeferWindowPos(hdwp, hwndHeap, NULL
		, 5
		, rcTab.bottom - rcTermBtn.bottom
		, (rcTab.right - rcTab.left) - rcTermBtn.right
		, rcTermBtn.bottom
		, SWP_NOZORDER);

	EndDeferWindowPos(hdwp);

	int cx = rcTab.right - rcTab.left;
	cx -= 2 * GetSystemMetrics(SM_CXBORDER);
	cx -= GetSystemMetrics(SM_CXVSCROLL);
	cx -= ListView_GetColumnWidth(hwndLView, 1) + 1;
	cx -= ListView_GetColumnWidth(hwndLView, 2) + 1;
	cx -= ListView_GetColumnWidth(hwndLView, 3) + 1;
	cx -= ListView_GetColumnWidth(hwndLView, 4) + 1;
	ListView_SetColumnWidth(hwndLView, 0, cx);
}

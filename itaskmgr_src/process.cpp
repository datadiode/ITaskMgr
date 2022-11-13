#include "stdafx.h"
#include "ITaskMgr.h"

static SIZE_T CalcHeapOfProcess(DWORD dwProcessID);
static BOOL DeleteProcessItem(HWND hwndLView, DWORD* pdwProcessIDs);
static BOOL InitProcessListViewColumns(HWND hwndLView);
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
	{
		HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS);
		InitProcessListViewColumns(hwndLView);
		DrawProcessView(hwndLView);

		ListView_SetExtendedListViewStyle(hwndLView, LVS_EX_FULLROWSELECT);

		pTP = (ThreadPack*)lParam;
		return TRUE;
	}

	// ----------------------------------------------------------
	case WM_NOTIFY:
		if( pTP == NULL )
			break;
		
		LPNMHDR lpnmhdr;
		lpnmhdr = (LPNMHDR)lParam;

		if( (lpnmhdr->hwndFrom == hDlg)
			&& (lpnmhdr->idFrom == IDC_LV_PROCESS) )
		{
			switch( lpnmhdr->code )
			{
			case 0:
			default:
				break;
			}
		}
		break; 

	// ----------------------------------------------------------
	case WM_COMMAND:
	{
		switch( LOWORD(wParam) )
		{
		case IDC_TERMINATE:
			HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS);
			KillSelectedProcess(hwndLView);
		}
		return 0;
	}
	// ----------------------------------------------------------
	case WM_SIZE:
	{
		if( pTP == NULL )
			return 0;
		ResizeWindow(hDlg, lParam);
		return 0;
	} 

	// ----------------------------------------------------------
	case WM_TIMER:
	{
		HWND hwndLView = GetDlgItem(hDlg, IDC_LV_PROCESS);
		HWND hwndHeap = GetDlgItem(hDlg, IDC_HEAP);
		TCHAR szFmt[128];
		
		DrawProcessView(hwndLView);

		DWORD dwProcessID = (DWORD)GetSelectedItemLParam(hwndLView);
		
		SIZE_T dwUsedHeap = CalcHeapOfProcess(dwProcessID);

		wsprintf(szFmt, _T("Memory used %uKB"), (DWORD)(dwUsedHeap>>10));
		SetWindowText(hwndHeap, szFmt);

		return 0;
	}
	}
	
	return FALSE;

}

//-----------------------------------------------------------------------------
// draw graph of process and memory
//-----------------------------------------------------------------------------
static BOOL DrawProcessView(HWND hwndLView)
{
	if(hwndLView == NULL)
	{
		return FALSE;
	}

	BOOL bRet;
	HANDLE hSS;

	hSS = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if( hSS == (void*)-1 )
	{
		return FALSE;
	}

	// -- process

	PROCESSENTRY32 pe32;
	memset(&pe32, 0, sizeof(PROCESSENTRY32));
	pe32.dwSize = sizeof(PROCESSENTRY32);

	bRet = Process32First(hSS, &pe32);

	if( pe32.dwSize != sizeof(PROCESSENTRY32) )
	{
		CloseToolhelp32Snapshot(hSS);
		return FALSE;
	}

	int nIndex[256];
	memset(&nIndex, 0, sizeof(int));

	DWORD dwProcessIDs[256];
	memset(&dwProcessIDs, 0, sizeof(DWORD)*256);

	int ii = 0;

	do
	{
		InsertProcessItem(hwndLView, &pe32);
		dwProcessIDs[ii++] = pe32.th32ProcessID;
	}while( Process32Next(hSS, &pe32) && ii < 255 );

	DeleteProcessItem(hwndLView, &dwProcessIDs[0]);

	CloseToolhelp32Snapshot(hSS);

	if( 0 == GetSelectedItemLParam(hwndLView) )
	{
		ListView_SetItemState(hwndLView, 0, LVIS_SELECTED, LVIS_SELECTED);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// make columns header
//-----------------------------------------------------------------------------
static BOOL InitProcessListViewColumns(HWND hwndLView)
{
	if( hwndLView == NULL )
		return FALSE;

	RECT rcLView;
	GetClientRect(hwndLView, &rcLView);


	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 

	// Process Image
	lvc.iSubItem = 0;
	lvc.pszText = _T("image name");
	lvc.fmt = LVCFMT_LEFT;
	if (ListView_InsertColumn(hwndLView, 0, &lvc) == -1) 
		return FALSE; 

	// Process ID
	lvc.iSubItem = 1;
	lvc.pszText = _T("id");
	lvc.fmt = LVCFMT_RIGHT;
	if (ListView_InsertColumn(hwndLView, 1, &lvc) == -1)
		return FALSE; 

	// Process Threads
	lvc.iSubItem = 2;
	lvc.pszText = _T("Threads");
	lvc.fmt = LVCFMT_RIGHT;
	if (ListView_InsertColumn(hwndLView, 2, &lvc) == -1) 
		return FALSE; 

	HDC hDC = GetDC(hwndLView);
	SIZE sz;
	GetTextExtentPoint(hDC, _T("0000000000"), 10, &sz);
	ReleaseDC(hwndLView, hDC);

	ListView_SetColumnWidth(hwndLView, 1, sz.cx);
	ListView_SetColumnWidth(hwndLView, 2, sz.cx/2);

    return TRUE; 
}

//-----------------------------------------------------------------------------
// fill process view
//-----------------------------------------------------------------------------
static BOOL InsertProcessItem(HWND hwndLView, PROCESSENTRY32* ppe32)
{
	if(hwndLView == NULL)
		return FALSE;

	// serch item
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

	// threads
	wsprintf( szFmt, _T("%d"), ppe32->cntThreads );
	ListView_SetItemText( hwndLView, dwIndex, 2, szFmt );

	return TRUE;
}

//-----------------------------------------------------------------------------
// clean up process which is not now on memory
//-----------------------------------------------------------------------------
static BOOL DeleteProcessItem(HWND hwndLView, DWORD* pdwProcessIDs)
{
	int nIndex = -1;
	int ii = 0;
	nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);

	LVITEM lvItem;
	memset(&lvItem, 0, sizeof(LVITEM));

	lvItem.mask = LVIF_PARAM;
	
	while( nIndex != -1 )
	{
		lvItem.iItem = nIndex;
		ListView_GetItem(hwndLView, &lvItem);

		ii = 0;
		BOOL fDelete = TRUE;
		do
		{
			if( pdwProcessIDs[ii++] == (DWORD)lvItem.lParam )
			{
				fDelete = FALSE;
				break;
			}
		}while( pdwProcessIDs[ii] );

		if( fDelete )
		{
			ListView_DeleteItem(hwndLView, nIndex);
			nIndex = -1;
		}

		nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);
	}
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
	ListView_SetColumnWidth(hwndLView, 0, cx);
}

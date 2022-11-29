#include "stdafx.h"
#include "ITaskMgr.h"

#ifndef _WIN32_WCE
class ID2TH
{
	HANDLE const h;
public:
	ID2TH(DWORD id) : h(OpenThread(THREAD_QUERY_INFORMATION, FALSE, id)) { }
	~ID2TH() { CloseHandle(h); }
	operator HANDLE() const { return h; }
};
#else
typedef HANDLE ID2TH;
#endif

static BOOL DeleteThreadItem(HWND hwndLView, DWORD* pdwThreadIDs);
static void InitThreadListViewColumns(HWND hwndLView);
static BOOL InsertThreadItem(HWND hDlg, HWND hwndLView, THREADENTRY32* pte32);
static BOOL DrawThreadView(HWND hwndLView, ThreadPack* pTP);
static void ResizeWindow(HWND hDlg, LPARAM lParam);

//-----------------------------------------------------------------------------
// process listview dialog
//-----------------------------------------------------------------------------
INT_PTR CALLBACK DlgProcThread(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;

	switch(Msg)
	{

	// ----------------------------------------------------------
	case WM_INITDIALOG:
		pTP = (ThreadPack*)lParam;
		if (HWND hwndLView = GetDlgItem(hDlg, IDC_LV_THREAD))
		{
			ListView_SetExtendedListViewStyle(hwndLView, LVS_EX_FULLROWSELECT);
			InitThreadListViewColumns(hwndLView);
		}
		return TRUE;

	// ----------------------------------------------------------
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom == IDC_LV_THREAD)
		{
			LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lParam;
			switch (lpnmlv->hdr.code)
			{
			case NM_DBLCLK:
				TabCtrl_SetCurSel(pTP->hwndTab, MODE_PROCESS);
				NMHDR nmh = { pTP->hwndTab, IDC_TAB, TCN_SELCHANGE };
				SendMessage(pTP->hDlg, WM_NOTIFY, 0, (LPARAM)&nmh);
				SetFocus(GetNextDlgTabItem(pTP->hwndProcessList, NULL, FALSE));
				break;
			}
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
			if (HWND hwndLView = GetDlgItem(hDlg, IDC_LV_THREAD))
			{
				DrawThreadView(hwndLView, pTP);
			}
		}
		break;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// draw graph of thread
//-----------------------------------------------------------------------------
static BOOL DrawThreadView(HWND hwndLView, ThreadPack* pTP)
{
	HANDLE const hSS = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pTP->dwSelectedProcessID);

	if (hSS == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	// -- thread

	THREADENTRY32 te32;
	te32.dwSize = sizeof te32;

	LPARAM lParam[PROCESS_MAX];

	int n = 0;

	if (Thread32First(hSS, &te32)) do
	{
		if (te32.th32OwnerProcessID == pTP->dwSelectedProcessID)
		{
			InsertThreadItem(pTP->hDlg, hwndLView, &te32);
			lParam[n++] = te32.th32ThreadID;
		}
	} while (Thread32Next(hSS, &te32) && (n < PROCESS_MAX));

	DeleteExcessItemsLParam(hwndLView, lParam, n);

	CloseToolhelp32Snapshot(hSS);

	return TRUE;
}

//-----------------------------------------------------------------------------
// make columns header
//-----------------------------------------------------------------------------
static void InitThreadListViewColumns(HWND hwndLView)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 

	// Thread ID/Owner
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("0000000000"));
	lvc.fmt = LVCFMT_LEFT;
	lvc.pszText = _T("id");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 0, &lvc);
	lvc.pszText = _T("owner");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 1, &lvc);

	// Thread Priority/Affinity
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("00000"));
	lvc.fmt = LVCFMT_RIGHT;
	lvc.pszText = _T("prio");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 2, &lvc);
	lvc.pszText = _T("affin");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 3, &lvc);

	// Thread Kernel/User Time
	lvc.cx = ListView_GetStringWidth(hwndLView, _T("000000000000000000000"));
	lvc.fmt = LVCFMT_RIGHT;
	lvc.pszText = _T("ktime");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 4, &lvc);
	lvc.pszText = _T("utime");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 5, &lvc);

	lvc.fmt = LVCFMT_LEFT;
	lvc.pszText = _T("name");
	ListView_InsertColumn(hwndLView, lvc.iSubItem = 6, &lvc);
}

static BOOL CALLBACK PropEnumProcEx(HWND hWnd, LPWSTR lpszString, HANDLE hData, ULONG_PTR dwData)
{
	if (LPWSTR lpszEquals = wcschr(lpszString, L'='))
	{
		HWND hwndLView = reinterpret_cast<HWND>(dwData);

		LVFINDINFO finditem;
		finditem.flags = LVFI_PARAM;
		finditem.lParam = reinterpret_cast<LPARAM>(hData);

		DWORD dwIndex = ListView_FindItem(hwndLView, -1, &finditem);
		if (dwIndex != -1)
		{
			ListView_SetItemText(hwndLView, dwIndex, 6, lpszEquals + 1);
		}
	}

	DWORD dwExitCode = 0;
	if (!GetExitCodeThread((ID2TH)reinterpret_cast<DWORD>(hData), &dwExitCode))
		RemoveProp(hWnd, lpszString); // This also implies a GlobalDeleteAtom()

	return TRUE;
}

//-----------------------------------------------------------------------------
// fill process view
//-----------------------------------------------------------------------------
static BOOL InsertThreadItem(HWND hDlg, HWND hwndLView, THREADENTRY32* pte32)
{
	// search item
	LVFINDINFO finditem;
	finditem.flags = LVFI_PARAM;
	finditem.lParam = pte32->th32ThreadID;

	int nIndex = ListView_FindItem(hwndLView, -1, &finditem);

	TCHAR szFmt[256];

	if (nIndex == -1)
	{
		LVITEM lvItem;
		memset(&lvItem, 0, sizeof lvItem);

		lvItem.mask = LVIF_TEXT | LVIF_PARAM;
		lvItem.iItem = 0;
		lvItem.iSubItem = 0;
		lvItem.pszText = szFmt;
		lvItem.lParam = pte32->th32ThreadID;

		wsprintf(szFmt, _T("%08X"), pte32->th32ThreadID);
		nIndex = ListView_InsertItem(hwndLView, &lvItem);

		if (nIndex == -1)
		{
			return FALSE;
		}

		// Add nonvolatile subitems

		// owner
		wsprintf(szFmt, _T("%08X"), pte32->th32OwnerProcessID);
		ListView_SetItemText(hwndLView, nIndex, 1, szFmt);
	}

	// Add volatile subitems

	// priority
	wsprintf(szFmt, _T("%d"), pte32->tpBasePri);
	ListView_SetItemText(hwndLView, nIndex, 2, szFmt);

	// affinity
	DWORD dwAffinity;
	if (CeGetThreadAffinity((HANDLE)pte32->th32ThreadID, &dwAffinity))
	{
		wsprintf(szFmt, _T("%02X"), dwAffinity);
		ListView_SetItemText(hwndLView, nIndex, 3, szFmt);
	}

	FILETIME ctime, etime, ktime, utime;
	if (GetThreadTimes((ID2TH)pte32->th32ThreadID, &ctime, &etime, &ktime, &utime))
	{
		// format to millisecond precision
		wsprintf(szFmt, _T("%u,%03u,%03u,%03u,%03u,%03u"),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E19) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E16) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E13) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E10) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E07) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(ktime) / static_cast<UINT64>(1E04) % 1000));
		ListView_SetItemText(hwndLView, nIndex, 4, szFmt);
		wsprintf(szFmt, _T("%u,%03u,%03u,%03u,%03u,%03u"),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E19) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E16) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E13) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E10) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E07) % 1000),
			static_cast<UINT>(reinterpret_cast<UINT64&>(utime) / static_cast<UINT64>(1E04) % 1000));
		ListView_SetItemText(hwndLView, nIndex, 5, szFmt);
	}

	EnumPropsEx(hDlg, PropEnumProcEx, reinterpret_cast<LPARAM>(hwndLView));

	return TRUE;
}

//-----------------------------------------------------------------------------
// Resize all window
//-----------------------------------------------------------------------------
static void ResizeWindow(HWND hDlg, LPARAM lParam)
{
	HWND hwndLView = GetDlgItem(hDlg, IDC_LV_THREAD);

	RECT rcTab;

	SetRect(&rcTab, 0, 0, LOWORD(lParam), HIWORD(lParam));
	TabCtrl_AdjustRect(hDlg, FALSE, &rcTab);

	HDWP hdwp = BeginDeferWindowPos(1);

	hdwp = DeferWindowPos(hdwp, hwndLView, NULL
		, rcTab.left
		, rcTab.top
		, rcTab.right - rcTab.left
		, rcTab.bottom - rcTab.top
		, SWP_NOZORDER);

	EndDeferWindowPos(hdwp);
}

#include "stdafx.h"
#include "ITaskMgr.h"

//-----------------------------------------------------------------------------
// info dialog proc
//-----------------------------------------------------------------------------
INT_PTR CALLBACK DlgProcInfo(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;

	switch (Msg)
	{

	// ----------------------------------------------------------
	case WM_INITDIALOG:
	{
		pTP = (ThreadPack*)lParam;

		HWND hwndText = GetDlgItem(hDlg, IDC_INFO_TEXT);
		static UINT const tabstop = 92;
		SendMessage(hwndText, EM_SETTABSTOPS, 1, (LPARAM)&tabstop);

		TCHAR szFmt[1024], *pszFmt = szFmt;
		pszFmt += wsprintf(pszFmt, _T("ProcessorArchitecture\t%u\r\n"), pTP->si.wProcessorArchitecture);
		pszFmt += wsprintf(pszFmt, _T("PageSize\t%u\r\n"), pTP->si.dwPageSize);
		pszFmt += wsprintf(pszFmt, _T("MinimumApplicationAddress\t%p\r\n"), pTP->si.lpMinimumApplicationAddress);
		pszFmt += wsprintf(pszFmt, _T("MaximumApplicationAddress\t%p\r\n"), pTP->si.lpMaximumApplicationAddress);
		pszFmt += wsprintf(pszFmt, _T("ActiveProcessorMask\t%IX\r\n"), pTP->si.dwActiveProcessorMask);
		pszFmt += wsprintf(pszFmt, _T("NumberOfProcessors\t%u\r\n"), pTP->si.dwNumberOfProcessors);
		pszFmt += wsprintf(pszFmt, _T("ProcessorType\t%u\r\n"), pTP->si.dwProcessorType);
		pszFmt += wsprintf(pszFmt, _T("AllocationGranularity\t%u\r\n"), pTP->si.dwAllocationGranularity);
		pszFmt += wsprintf(pszFmt, _T("ProcessorLevel\t%u\r\n"), pTP->si.wProcessorLevel);
		pszFmt += wsprintf(pszFmt, _T("ProcessorRevision\t%u\r\n"), pTP->si.wProcessorRevision);
		SetWindowText(hwndText, szFmt);

		return TRUE;
	}

	// ----------------------------------------------------------
	case WM_SIZE:
	{
		HWND hwndText = GetDlgItem(hDlg, IDC_INFO_TEXT);

		RECT rcTab;

		SetRect(&rcTab, 0, 0, LOWORD(lParam), HIWORD(lParam));
		TabCtrl_AdjustRect(hDlg, FALSE, &rcTab);

		HDWP hdwp = BeginDeferWindowPos(1);

		hdwp = DeferWindowPos(hdwp, hwndText, NULL
			, rcTab.left
			, rcTab.top
			, rcTab.right - rcTab.left
			, rcTab.bottom - rcTab.top
			, SWP_NOZORDER);

		EndDeferWindowPos(hdwp);
		return 0;
	}

	default:
		break;

	}
	return 0;
}

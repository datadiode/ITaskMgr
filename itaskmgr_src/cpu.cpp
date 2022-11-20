#include "stdafx.h"
#include "ITaskMgr.h"

static BOOL ShowCpuStatus(ThreadPack* pTP);
static BOOL DrawGraph(ThreadPack* pTP, HWND hwndDraw);

static HPEN hpenDarkGreen;
static HPEN hpenLightGreen;
static HPEN hpenMagenta;
static HPEN hpenYellow;

//-----------------------------------------------------------------------------
// cpu graph dialog proc
//-----------------------------------------------------------------------------
INT_PTR CALLBACK DlgProcCpu(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;

	switch(Msg)
	{
	
	// ----------------------------------------------------------
	case WM_INITDIALOG:
	{
		pTP = (ThreadPack*)lParam;

		hpenDarkGreen = CreatePen(PS_SOLID, 1, RGB(0, 127, 0));
		hpenLightGreen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
		hpenMagenta = CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
		hpenYellow = CreatePen(PS_SOLID, 1, RGB(255, 255, 0));

		HWND hwndText = GetDlgItem(hDlg, IDC_CPU_TEXT);
		static UINT const tabstop = 20;
		SendMessage(hwndText, EM_SETTABSTOPS, 1, (LPARAM)&tabstop);

		HWND hwndDraw = GetDlgItem(hDlg, IDC_CPU_DRAW);
		DrawGraph(pTP, hwndDraw);
		ShowCpuStatus(pTP);
		return TRUE;
	}

	// ----------------------------------------------------------
	case WM_DESTROY:
	{
		DeleteObject(hpenDarkGreen);
		DeleteObject(hpenLightGreen);
		DeleteObject(hpenMagenta);
		DeleteObject(hpenYellow);
		break;
	}

	// ----------------------------------------------------------
	case WM_SIZE:
	{ 
		HWND hwndDraw = GetDlgItem(hDlg, IDC_CPU_DRAW);
		HWND hwndText = GetDlgItem(hDlg, IDC_CPU_TEXT);

		RECT rcTab;
		RECT rcDraw;
		RECT rcTitle;

		SetRect(&rcTab, 0, 0, LOWORD(lParam), HIWORD(lParam));
		TabCtrl_AdjustRect(hDlg, FALSE, &rcTab);

		GetClientRect(hwndDraw, &rcDraw);
		GetClientRect(hwndText, &rcTitle);

		HDWP hdwp = BeginDeferWindowPos(2);
		
		hdwp = DeferWindowPos(hdwp, hwndDraw, NULL
			, rcTab.left
			, rcTab.top
			, rcTab.right - rcTab.left
			, rcTab.bottom - rcTitle.bottom
			, SWP_NOZORDER);

		hdwp = DeferWindowPos(hdwp, hwndText, NULL
			, rcTab.left
			, rcTab.bottom - rcTitle.bottom
			, rcTab.right - rcTab.left
			, rcTitle.bottom
			, SWP_NOZORDER);

		EndDeferWindowPos(hdwp);
		ShowCpuStatus(pTP);
		return 0;
	} 
	
	// ----------------------------------------------------------
	case WM_DRAWITEM:
	{
		LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
		DrawGraph(pTP, lpdis->hwndItem);
		return TRUE;
	}

	case WM_TIMER:
	{
		ShowCpuStatus(pTP);
		InvalidateRect(GetDlgItem(hDlg, IDC_CPU_DRAW), NULL, FALSE);
		break;
	}

	default:
		break;

	}
	return FALSE;

}

//-----------------------------------------------------------------------------
// show cpu status text
//-----------------------------------------------------------------------------
static BOOL ShowCpuStatus(ThreadPack* pTP)
{
	if( pTP == NULL )
		return FALSE;

	MEMORYSTATUS ms;
	TCHAR szTmp[64];
	TCHAR szFmt[1024], *pszFmt = szFmt;

	HWND hDlg;
	HWND hwndStatus;

	if( (hDlg = pTP->hwndCpupower ) == NULL )
		return FALSE;
	if( (hwndStatus = GetDlgItem(hDlg, IDC_CPU_TEXT) ) == NULL )
		return FALSE;

	ms.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&ms);

	SIZE_T dwTotalMem = ms.dwTotalPhys>>10;
	SIZE_T dwUsedMem = dwTotalMem - (ms.dwAvailPhys >> 10);

	pszFmt += wsprintf(pszFmt, _T("CPU time\t"));
	for (DWORD i = 0; i < pTP->si.dwNumberOfProcessors; ++i)
		pszFmt += wsprintf(pszFmt, _T("\t%d%%"), pTP->chPowHistory[0][i]);

	pszFmt += wsprintf(pszFmt, _T("\r\nMemory used\t"));
	GetNumberFormat(LOCALE_INVARIANT, 0, szTmp, NULL, pszFmt, wsprintf(szTmp, _T("%u"), dwUsedMem) + 10);
	pszFmt += wsprintf(pszFmt = _tcschr(pszFmt, '.'), _T(" KB / "));
	GetNumberFormat(LOCALE_INVARIANT, 0, szTmp, NULL, pszFmt, wsprintf(szTmp, _T("%u"), dwTotalMem) + 10);
	pszFmt += wsprintf(pszFmt = _tcschr(pszFmt, '.'), _T(" KB"));

	SetWindowText(hwndStatus, szFmt);
	return TRUE;
}

//-----------------------------------------------------------------------------
// draw graph
//-----------------------------------------------------------------------------
static BOOL DrawGraph(ThreadPack* pTP, HWND hwndDraw)
{
	int i, ii;

	HDC hDC;
	RECT rc;

	HPEN hOldPen;

	POINT pntBuf[HISTORY_MAX];
	memset( pntBuf, 0, sizeof(POINT)*HISTORY_MAX );

	GetClientRect(hwndDraw, &rc);

	if( pTP->nMode != MODE_CPUPOWER )
		return TRUE;

	static int nXLine = 0;
	nXLine = (nXLine+=2) %12;

	// drawing

	if(!(hDC = GetDC(hwndDraw)))
	{
		return FALSE;
	}

	// paint background
	SelectObject(hDC, GetStockObject(BLACK_BRUSH));

	Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);

	hOldPen = (HPEN)SelectObject(hDC, hpenDarkGreen);

	POINT pnt[2];
	for( ii = 0; ii < rc.bottom - rc.top; ii+=12 )
	{
		pnt[0].x = rc.left;
		pnt[1].x = rc.right;
		pnt[0].y = pnt[1].y = ii + rc.top;
		Polyline(hDC, &pnt[0], 2);
	}

	for( ii = 12; ii < (rc.right - rc.left) + nXLine; ii+=12 )
	{
		pnt[0].x = pnt[1].x = ii + rc.left - nXLine;
		pnt[0].y = rc.top;
		pnt[1].y = rc.bottom;
		Polyline(hDC, &pnt[0], 2);
	}

	SelectObject(hDC, hOldPen);
	POINT* pPoint;
	int xx;

	// Draw CPU POWER
	for (i = pTP->si.dwNumberOfProcessors; i--;)
	{
		pPoint = &pntBuf[0];

		xx = rc.right;
		for (ii = 0; (ii < HISTORY_MAX) && (xx > rc.left); ii++, xx -= 2, pPoint++)
		{
			LONG lHeight = (100 - pTP->chPowHistory[ii][i]);
			pPoint->x = xx;
			pPoint->y = rc.top + lHeight * (rc.bottom - rc.top) / 100;
		}

		hOldPen = (HPEN)SelectObject(hDC, i == 0 ? hpenLightGreen : hpenMagenta);
		Polyline(hDC, &pntBuf[0], ii);
		SelectObject(hDC, hOldPen);
	}

	// Draw Memory load
	pPoint = &pntBuf[0];

	xx = rc.right;
	for( ii = 0; (ii < HISTORY_MAX) && (xx > rc.left); ii++, xx-=2, pPoint++ )
	{
		LONG lHeight = (100 - pTP->chMemHistory[ii]);
		pPoint->x = xx;
		pPoint->y = rc.top + lHeight * (rc.bottom - rc.top)/100 ;
	}

	hOldPen = (HPEN)SelectObject(hDC, hpenYellow);
	Polyline(hDC, &pntBuf[0], ii);
	SelectObject(hDC, hOldPen);

	// finish
	SelectObject(hDC, hOldPen);
	ReleaseDC(hwndDraw, hDC);

	return TRUE;
}

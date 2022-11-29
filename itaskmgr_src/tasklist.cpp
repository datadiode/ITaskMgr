#include "stdafx.h"
#include "ITaskMgr.h"
#include "resource.h"

static void InitTaskListViewColumns(HWND hwndLView);
static void DrawTaskView(HWND hwndLView);
static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
static BOOL InsertTaskItem(HWND hwndLView, HWND hwndEnum);
static void ResizeWindow(HWND hDlg, LPARAM lParam);

static HIMAGELIST g_himlIcons;
static HICON g_hDefaultIcon;
static int g_nDefaultIconIndex;

//-----------------------------------------------------------------------------
// process listview dialog
//-----------------------------------------------------------------------------
INT_PTR CALLBACK DlgProcTask(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static ThreadPack* pTP = NULL;

	switch(Msg)
	{

	// ----------------------------------------------------------
	case WM_INITDIALOG:
		pTP = (ThreadPack*)lParam;

		if (HWND hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST))
		{
			int const cx = GetSystemMetrics(SM_CXSMICON);
			int const cy = GetSystemMetrics(SM_CYSMICON);

			g_himlIcons = ImageList_Create(cx, cy, ILC_MASK, 1, 1);
			g_hDefaultIcon = (HICON)LoadImage(pTP->g_hInst, MAKEINTRESOURCE(IDI_DEFAULT_SMALLICON), IMAGE_ICON, cx, cy, 0);
			g_nDefaultIconIndex = ImageList_AddIcon(g_himlIcons, g_hDefaultIcon);

			ListView_SetExtendedListViewStyle(hwndLView, LVS_EX_FULLROWSELECT);
			ListView_SetImageList(hwndLView, g_himlIcons, LVSIL_SMALL);
			InitTaskListViewColumns(hwndLView);
		}
		return TRUE;

	// ----------------------------------------------------------
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom == IDC_LV_TASKLIST)
		{
			LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lParam;
			LVITEM lvItem;
			switch (lpnmlv->hdr.code)
			{
			case NM_DBLCLK:
				if (HWND hwndTarget = (HWND)GetSelectedItemLParam(lpnmlv->hdr.hwndFrom))
				{
					DWORD dwProcessID;
					if (GetWindowThreadProcessId(hwndTarget, &dwProcessID))
					{
						TabCtrl_SetCurSel(pTP->hwndTab, MODE_PROCESS);
						NMHDR nmh = { pTP->hwndTab, IDC_TAB, TCN_SELCHANGE };
						SendMessage(pTP->hDlg, WM_NOTIFY, 0, (LPARAM)&nmh);
						HWND hwndProcessList = GetDlgItem(pTP->hwndProcessList, IDC_LV_PROCESS);
						SelectItemLParam(hwndProcessList, dwProcessID);
						SetFocus(hwndProcessList);
					}
				}
				break;

			case LVN_DELETEITEM:
				lvItem.mask = LVIF_IMAGE;
				lvItem.iItem = lpnmlv->iItem;
				lvItem.iSubItem = 0;
				ListView_GetItem(lpnmlv->hdr.hwndFrom, &lvItem);
				int nDeleteIconIndex = lvItem.iImage;
				if (nDeleteIconIndex != g_nDefaultIconIndex)
				{
					ImageList_Remove(g_himlIcons, nDeleteIconIndex);
					// refresh icon
					int nIndex = -1;
					while ((nIndex = ListView_GetNextItem(lpnmlv->hdr.hwndFrom, nIndex, 0)) != -1)
					{
						lvItem.iItem = nIndex;
						ListView_GetItem(lpnmlv->hdr.hwndFrom, &lvItem);
						if (lvItem.iImage >= nDeleteIconIndex)
						{
							lvItem.iImage--;
							ListView_SetItem(lpnmlv->hdr.hwndFrom, &lvItem);
						}
					}
				}
				break;
			}
		}
		break; 

	// ----------------------------------------------------------
	case WM_COMMAND:
		if (HWND hwndTarget = (HWND)GetSelectedItemLParam(GetDlgItem(hDlg, IDC_LV_TASKLIST)))
		{
			switch (LOWORD(wParam))
			{
			case IDC_TASK_CLOSE:
				PostMessage(hwndTarget, WM_CLOSE, 0, 0);
				break;
		
			case IDC_TASK_SWITCH:
				ShowWindow(pTP->hDlg, SW_MINIMIZE);
				SetForegroundWindow(hwndTarget);
				break;
			}
		}
		break;

	// ----------------------------------------------------------
	case WM_SIZE:
		ResizeWindow(hDlg, lParam);
		break;

	// ----------------------------------------------------------
	case WM_DESTROY:
		ImageList_Destroy(g_himlIcons);
		break;

	// ----------------------------------------------------------
	case WM_WINDOWPOSCHANGED:
		if (((LPWINDOWPOS)lParam)->flags & (SWP_SHOWWINDOW | SWP_FRAMECHANGED))
		{
			HWND hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST);
			DrawTaskView(hwndLView);
		}
		break;
	}

	return FALSE;
}


//-----------------------------------------------------------------------------
// make columns header
//-----------------------------------------------------------------------------
static void InitTaskListViewColumns(HWND hwndLView)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 

	// Process Image
	lvc.iSubItem = 0;
	lvc.pszText = _T("Task");
	lvc.fmt = LVCFMT_LEFT;

	ListView_InsertColumn(hwndLView, 0, &lvc);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static void DrawTaskView(HWND hwndLView)
{
	HWND hwndEnum[256];
	memset(hwndEnum, 0, sizeof hwndEnum);
	EnumWindows(EnumWindowsProc, (LPARAM)hwndEnum);

	int n = 0;
	while (n < _countof(hwndEnum) && hwndEnum[n])
	{
		InsertTaskItem(hwndLView, hwndEnum[n++]);
	}

	DeleteExcessItemsLParam(hwndLView, (LPARAM*)hwndEnum, n);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	TCHAR szBuf[256];
	HWND* pWnd;

	pWnd = (HWND*)lParam;

	GetWindowText(hWnd, szBuf, 4);
	
	// --
	if( !IsWindowVisible(hWnd) )
		return TRUE;

	if( *szBuf == '\0' )
		return TRUE;

	GetClassName( hWnd, szBuf, MAX_PATH );
	if( 0 == _tcscmp(szBuf, _T("DesktopExplorerWindow") ) )
		return TRUE;

	// ITaskMgr can show upper 256 window.
	for( int ii = 0; ii < 256 && *pWnd != 0; ii++, pWnd++);

	*pWnd = hWnd;

	return TRUE;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static BOOL InsertTaskItem(HWND hwndLView, HWND hwndEnum)
{
	// serch item
	LVFINDINFO finditem;
	finditem.flags = LVFI_PARAM;
	finditem.lParam = (LPARAM)hwndEnum;

	int nItemIndex = ListView_FindItem(hwndLView, -1, &finditem);

	if (nItemIndex != -1)
	{
		return FALSE;
	}

	LVITEM lvItem;
	memset(&lvItem, 0, sizeof lvItem);

	TCHAR szWinText[256];
	GetWindowText(hwndEnum, szWinText, _countof(szWinText));

	int nIconIndex = -1;
	if (HICON hIcon = (HICON)SendMessage(hwndEnum, WM_GETICON, ICON_SMALL, ICON_SMALL))
	{
		nIconIndex = ImageList_AddIcon(g_himlIcons, hIcon);
	}

	lvItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvItem.iImage = nIconIndex != -1 ? nIconIndex : g_nDefaultIconIndex;
	lvItem.iItem = 0xFF;
	lvItem.iSubItem = 0;
	lvItem.pszText = szWinText;
	lvItem.lParam = (LPARAM)hwndEnum;

	nItemIndex = ListView_InsertItem(hwndLView, &lvItem);

	if (nItemIndex == -1)
	{
		return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Resize all window
//-----------------------------------------------------------------------------
static void ResizeWindow(HWND hDlg, LPARAM lParam)
{
	HWND hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST);
	HWND hwndTaskSwitch = GetDlgItem(hDlg, IDC_TASK_SWITCH);
	HWND hwndTaskClose = GetDlgItem(hDlg, IDC_TASK_CLOSE);

	RECT rcTab;
	RECT rcLView;
	RECT rcTaskSwitch;
	RECT rcTaskClose;

	SetRect(&rcTab, 0, 0, LOWORD(lParam), HIWORD(lParam));
	TabCtrl_AdjustRect(hDlg, FALSE, &rcTab);

	GetClientRect(hwndTaskSwitch, &rcTaskSwitch);
	GetClientRect(hwndTaskClose, &rcTaskClose);

	HDWP hdwp = BeginDeferWindowPos(3);
	
	hdwp = DeferWindowPos(hdwp, hwndLView, NULL
		, rcTab.left
		, rcTab.top
		, rcTab.right - rcTab.left
		, rcTab.bottom - rcTaskSwitch.bottom
		, SWP_NOZORDER);

	hdwp = DeferWindowPos(hdwp, hwndTaskSwitch, NULL
		, rcTab.right - rcTaskSwitch.right
		, rcTab.bottom - rcTaskSwitch.bottom
		, rcTaskSwitch.right
		, rcTaskSwitch.bottom
		, SWP_NOZORDER);

	hdwp = DeferWindowPos(hdwp, hwndTaskClose, NULL
		, rcTab.right - rcTaskSwitch.right - rcTaskClose.right
		, rcTab.bottom - rcTaskSwitch.bottom
		, rcTaskClose.right
		, rcTaskSwitch.bottom
		, SWP_NOZORDER);

	EndDeferWindowPos(hdwp);

	GetClientRect(hwndLView, &rcLView);
	ListView_SetColumnWidth(hwndLView, 0, rcLView.right);
}

#include "stdafx.h"
#include "ITaskMgr.h"
#include "resource.h"

static BOOL InitTaskListViewColumns(HWND hwndLView);
static BOOL DrawTaskView(HWND hwndLView);
static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
static BOOL InsertTaskItem(HWND hwndLView, HWND hwndEnum);
static BOOL DeleteTaskItem(HWND hwndLView, HWND* phwndEnumWins);
static void ResizeWindow(HWND hDlg, LPARAM lParam);

extern HINSTANCE	g_hInst;
HIMAGELIST g_himlIcons;
HICON g_hDefaultIcon;
DWORD g_nDefaultIconIndex;

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
	{
		g_himlIcons = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK, 1, 1);
		if( !g_himlIcons )
			return FALSE;

		g_hDefaultIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_DEFAULT_SMALLICON),
			IMAGE_ICON, SM_CYSMICON, SM_CYSMICON, 0);
		
		if( !g_hDefaultIcon )
			return FALSE;

		g_nDefaultIconIndex = ImageList_AddIcon(g_himlIcons, g_hDefaultIcon);

		if( g_nDefaultIconIndex == -1 )
			return FALSE;

		HWND hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST);
		ListView_SetExtendedListViewStyle(hwndLView, LVS_EX_FULLROWSELECT);
		InitTaskListViewColumns(hwndLView);
		DrawTaskView(hwndLView);
		pTP = (ThreadPack*)lParam;
		return TRUE;
	}

	// ----------------------------------------------------------
	case WM_NOTIFY:
	{
		if( pTP == NULL )
			break;
		
		LPNMHDR lpnmhdr;
		lpnmhdr = (LPNMHDR)lParam;
		HWND hwndTaskList = GetDlgItem(hDlg, IDC_LV_TASKLIST);

		if( (lpnmhdr->hwndFrom == hwndTaskList)
			&& (lpnmhdr->idFrom == IDC_LV_TASKLIST) )
		{

			switch( lpnmhdr->code )
			{
			case NM_DBLCLK:
			{
				LPARAM lParam;
				HWND hwndProcessList;
				DWORD dwProcessID;
				NMHDR nmh;
				
				lParam = GetSelectedItemLParam(hwndTaskList);
				
				GetWindowThreadProcessId((HWND)lParam, &dwProcessID);
				hwndProcessList = GetDlgItem(pTP->hwndProcessList, IDC_LV_PROCESS);
				
				SelectItemLParam(hwndProcessList, dwProcessID);
				TabCtrl_SetCurSel(pTP->hwndTab, MODE_PROCESS);

				nmh.hwndFrom = pTP->hwndTab;
				nmh.idFrom = IDC_TAB;
				nmh.code = TCN_SELCHANGE;
				SendMessage(pTP->hDlg, WM_NOTIFY, 0, (LPARAM)&nmh);
				SetFocus(hwndProcessList);

				return 0;
			}

			case NM_CLICK:
				break;

			default:
				break;
			}
		}
		break; 
	}

	// ----------------------------------------------------------
	case WM_COMMAND:
	{
		HWND hwndLView, hwndTarget;
		hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST);
		hwndTarget = (HWND)GetSelectedItemLParam(hwndLView);

		switch( LOWORD(wParam) )
		{
		case IDC_TASK_CLOSE:
			if( hwndTarget )
				PostMessage(hwndTarget, WM_CLOSE, 0, 0);
			break;
		
		case IDC_TASK_SWITCH:
			if( hwndTarget )
			{
				ShowWindow(pTP->hDlg, SW_MINIMIZE);
				SetForegroundWindow(hwndTarget);
			}
			break;

		default:
			break;
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
	case WM_DESTROY:
		ImageList_Destroy(g_himlIcons);
		return 0;

	// ----------------------------------------------------------
	case WM_TIMER:
	{
		HWND hwndLView = GetDlgItem(hDlg, IDC_LV_TASKLIST);
		DrawTaskView(hwndLView);
		return 0;
	}

	// ----------------------------------------------------------
	}	
	return FALSE;

}


//-----------------------------------------------------------------------------
// make columns header
//-----------------------------------------------------------------------------
static BOOL InitTaskListViewColumns(HWND hwndLView)
{
	if( hwndLView == NULL )
		return FALSE;

	if(g_himlIcons == NULL)
		return FALSE;
	
	ListView_SetImageList(hwndLView, g_himlIcons, LVSIL_SMALL); 

	RECT rcLView;
	GetClientRect(hwndLView, &rcLView);

	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 

	// Process Image
	lvc.iSubItem = 0;
	lvc.pszText = _T("Task");
	lvc.fmt = LVCFMT_LEFT;

	if (ListView_InsertColumn(hwndLView, 0, &lvc) == -1)
		return FALSE;

	ListView_SetColumnWidth(hwndLView, 0, rcLView.right-20);

    return TRUE; 
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static BOOL DrawTaskView(HWND hwndLView)
{
	HWND hwndEnum[256];
	memset(hwndEnum, 0, sizeof(HWND)*256);
	EnumWindows(EnumWindowsProc, (LPARAM)hwndEnum);

	HWND* pWnd = hwndEnum;

	for( int ii = 0; ii < 256 && *pWnd != 0; ii++, pWnd++)
	{
		InsertTaskItem(hwndLView, *pWnd);
	}

	DeleteTaskItem(hwndLView, hwndEnum);

	if( 0 == GetSelectedItemLParam(hwndLView) )
	{
		ListView_SetItemState(hwndLView, 0, LVIS_SELECTED, LVIS_SELECTED);
	}

	return TRUE;
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
	if(hwndLView == NULL || hwndEnum == NULL)
		return FALSE;

	DWORD dwItemIndex;
	DWORD dwIconIndex;
	TCHAR szWinText[256];

	// serch item
	LVFINDINFO finditem;
	memset(&finditem, 0, sizeof(LVFINDINFO));
	
	finditem.flags = LVFI_PARAM;
	finditem.lParam = (LPARAM)hwndEnum;

	dwItemIndex = ListView_FindItem(hwndLView, -1, &finditem);

	if( dwItemIndex != -1 )
	{
		return FALSE;
	}

	LVITEM lvItem;
	memset(&lvItem, 0, sizeof(LVITEM));
	GetWindowText(hwndEnum, szWinText, 256);

	HICON hIcon;
	hIcon = (HICON)SendMessage(hwndEnum, WM_GETICON, ICON_SMALL, ICON_SMALL);
	
	if( hIcon == NULL )
	{
		dwIconIndex = g_nDefaultIconIndex;
	}
	else
	{
		dwIconIndex = ImageList_AddIcon(g_himlIcons, hIcon);
	}

	if( dwIconIndex == -1 )
	{
		dwIconIndex = g_nDefaultIconIndex;
	}

	lvItem.mask = LVIF_TEXT|LVIF_PARAM|LVIF_IMAGE;
	lvItem.iImage = dwIconIndex;
	lvItem.iItem = 0xFF;
	lvItem.iSubItem = 0;
	lvItem.pszText = szWinText;
	lvItem.lParam = (LPARAM)hwndEnum;

	dwItemIndex = ListView_InsertItem(hwndLView, &lvItem);

	if( dwItemIndex == -1 )
	{
		return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static BOOL DeleteTaskItem(HWND hwndLView, HWND* phwndEnumWins)
{
	int nIndex = -1;
	int ii = 0;
	nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);

	LVITEM lvItem;
	memset(&lvItem, 0, sizeof(LVITEM));

	lvItem.mask = LVIF_PARAM|LVIF_IMAGE;
	
	while( nIndex != -1 )
	{
		lvItem.iItem = nIndex;
		ListView_GetItem(hwndLView, &lvItem);

		ii = 0;
		BOOL fDelete = TRUE;
		do
		{
			if( phwndEnumWins[ii++] == (HWND)lvItem.lParam )
			{
				fDelete = FALSE;
				break;
			}
		}while( phwndEnumWins[ii] );

		if( fDelete )
		{
			DWORD dwDeleteIndex = nIndex;
			DWORD dwDelteIconIndex = lvItem.iImage;

			ListView_DeleteItem(hwndLView, dwDeleteIndex);
			
			if( dwDelteIconIndex != g_nDefaultIconIndex )
			{
				ImageList_Remove(g_himlIcons, dwDelteIconIndex);

				// reflesh icon
				nIndex = ListView_GetNextItem(hwndLView, -1, 0);

				while( nIndex != -1 )
				{
					lvItem.iItem = nIndex;
					ListView_GetItem(hwndLView, &lvItem);
					
					if( lvItem.iImage >= (LPARAM)dwDelteIconIndex )
					{
						lvItem.iImage--;
						ListView_SetItem(hwndLView, &lvItem);
					}
					nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);
				}
			}
			
			nIndex = -1;
		}

		nIndex = ListView_GetNextItem(hwndLView, nIndex, 0);
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

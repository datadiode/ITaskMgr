#if !defined(__ITASKMGR_H__INCLUDED_)
#define __ITASKMGR_H__INCLUDED_

#define APPNAME	_T("ITaskMgr")
#define HISTORY_MAX	512
#define CPUCORE_MAX	8

#define MODE_ICON		-1
#define MODE_CPUPOWER	0
#define MODE_PROCESS	1
#define MODE_TASKLIST	2
#define MODE_INFO		3

#define MY_NOTIFYICON			(WM_APP + 1000)

typedef struct _ThreadPack
{
	HINSTANCE g_hInst;
	HWND hDlg;
	UINT nMode;
	SYSTEM_INFO si;
	BOOL bEnd;

	HANDLE hIdleThread[CPUCORE_MAX];
	
	HWND hwndTab;
	HWND hwndStayOnTop;
	HWND hwndProcessList;
	HWND hwndCpupower;
	HWND hwndTaskList;
	HWND hwndInfo;

	DWORD dwInterval;
	HICON hIcon[12];
	NOTIFYICONDATA	nidTrayIcon;

	char chPowHistory[HISTORY_MAX][CPUCORE_MAX];
	char chMemHistory[HISTORY_MAX];
} ThreadPack;

LPARAM GetSelectedItemLParam(HWND hwndLView);
void SelectItemLParam(HWND hwndLView, LPARAM lParam);

#endif // !defined(__ITASKMGR_H__INCLUDED_)

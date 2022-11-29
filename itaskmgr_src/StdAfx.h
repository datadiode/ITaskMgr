#if (_WIN32_WCE == 0x800) && defined(_X86_)
// To compensate for Compact2013_SDK_86Duino_80B's lack of CE_MODULES_COMMCTRL:
// - Use Header from <../../../../wce600/Beckhoff_HMI_600/Include/X86/commctrl.h>
// - Use ImpLib from $(SdkRootPath)..\..\..\wce600\Beckhoff_HMI_600\Lib\x86\commctrl.lib
#pragma include_alias(<commctrl.h>,<../../../../wce600/Beckhoff_HMI_600/Include/X86/commctrl.h>)
#endif

#if (_WIN32_WCE == 0x800)
// To compensate for SDK's lack of COREDLL_SHELLAPIS:
// - Use Header from <../../../../wce600/Beckhoff_HMI_600/Include/X86/shellapi.h>
// - Because there is no dedicated ImpLib, GetProcAddress() stuff from COREDLL.DLL
#pragma include_alias(<shellapi.h>,<../../../../wce600/Beckhoff_HMI_600/Include/X86/shellapi.h>)
#endif

#include <windows.h>
#include <commctrl.h>
#include <Tlhelp32.h>
#include <tchar.h>
#include "resource.h"

#undef GlobalAddAtom // GetProcAddress() it so as to not require GWES_ATOM

#ifndef _WIN32_WCE
#include <psapi.h>
#define GetProcAddressA GetProcAddress
#define CloseToolhelp32Snapshot CloseHandle
#define Heap32First(hSnapshot, lphe, th32ProcessID, th32HeapID) Heap32First(lphe, th32ProcessID, th32HeapID)
#define Heap32Next(hSnapshot, lphe) Heap32Next(lphe)
#define CeGetProcessAffinity(hThread, lpProcessAffinity) (FALSE)
#define CeGetThreadAffinity(hThread, lpProcessAffinity) (FALSE)
#define CeSetThreadAffinity(hThread, dwProcessor) SetThreadAffinityMask(hThread, dwProcessor)
#else
#define WC_DIALOG L"Dialog"
#if _WIN32_WCE <= 0x600
#define CeGetProcessAffinity(hThread, lpProcessAffinity) (FALSE)
#define CeGetThreadAffinity(hThread, lpProcessAffinity) (FALSE)
#define CeSetThreadAffinity(hThread, dwProcessor) (FALSE)
#endif
#endif

template<typename f>
struct DllImport {
	FARPROC p;
	f operator*() const { return reinterpret_cast<f>(p); }
};

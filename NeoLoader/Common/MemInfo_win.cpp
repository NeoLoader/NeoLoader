#include "GlobalHeader.h"
#include "MemInfo.h"

#ifdef WIN32

#include <windows.h>
#include <process.h>

typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD cb;
    DWORD PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;

typedef PROCESS_MEMORY_COUNTERS *PPROCESS_MEMORY_COUNTERS;
typedef	BOOL (WINAPI *pGetProcessMemoryInfo)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

struct SMemInfo
{
	HMODULE					PsapiDll;
	pGetProcessMemoryInfo	GetProcessMemoryInfo;
};

CMemInfo::CMemInfo()
{
	m_p = new SMemInfo;

	m_p->PsapiDll = ::LoadLibrary(L"psapi.dll");
	m_p->GetProcessMemoryInfo = m_p->PsapiDll ? (pGetProcessMemoryInfo)::GetProcAddress(m_p->PsapiDll, "GetProcessMemoryInfo") : NULL;
}

CMemInfo::~CMemInfo()
{
	delete m_p;
}

quint64 CMemInfo::Get()
{
	if (!m_p->GetProcessMemoryInfo)
		return 0;
	
	// Get a handle to the process.
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,getpid());
	if (!hProcess)
		return 0;
	
	PROCESS_MEMORY_COUNTERS ProcMemCounters;
	m_p->GetProcessMemoryInfo(hProcess, &ProcMemCounters, sizeof(ProcMemCounters));
	CloseHandle(hProcess);
	return ProcMemCounters.WorkingSetSize;
}

#endif
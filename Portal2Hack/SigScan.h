#ifndef SIGSCAN_H
#define SIGSCAN_H

#define WIN32_LEAN_AND_MEAN

#include <fstream>
#include <tchar.h>
#include <Windows.h>
#include <TlHelp32.h>

class SigScan
{
public:
	SigScan(TCHAR* exeName, TCHAR* modName = TEXT(""));
	~SigScan();

	BOOL FindProcessId();
	void SetProcessId(DWORD processId);

	BOOL OpenProcess();

	BOOL FindModInfo();

	BOOL Scan(TCHAR* sig, UINT_PTR* &results, DWORD size, DWORD& count);
	BOOL Scan(TCHAR* sig, UINT_PTR* result);

	DWORD GetProcessId() const;
	UINT_PTR GetBaseAddr() const;
	HANDLE GetProcess() const;
	DWORD GetBaseSize() const;

private:
	TCHAR* m_exeName;
	TCHAR* m_modName;

	DWORD m_processId;
	HANDLE m_process;

	UINT_PTR m_baseAddr;
	DWORD m_baseSize;
};

#endif // SIGSCAN_H
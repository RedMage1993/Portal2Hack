//*********************************************************
// Author: Fritz Ammon
// Date: 17 July 2011
// Program: RunOnce.h
// Description: Allows a programmer to limit a user to one
// instance of the application per session.
//*********************************************************

#ifndef RUNONCE_H
#define RUNONCE_H

#pragma comment (lib, "Shlwapi.lib")

#define WIN32_LEAN_AND_MEAN

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <Windows.h>

using std::ios;

#ifdef UNICODE
using std::wifstream;
typedef wifstream _tifstream;
#else
using std::ifstream;
typedef ifstream _tifstream;
#endif

class RunOnce
{
public:
	RunOnce(HWND hWnd);
	~RunOnce();

	BOOL GetMutexPathFile(TCHAR* lpFile = NULL);
	BOOL GetMutexName();
	BOOL MutexNameExists();
	BOOL GenMutexName();
	BOOL SetMutexName();
	BOOL SetPageWindow();
	BOOL BringToFocus();

	BOOL PerformCheck(TCHAR* lpFile = NULL);

private:
	TCHAR* m_pszMutexPathFile;
	TCHAR* m_pszMutexName;
	HANDLE m_hMutexFile;
	HANDLE m_hMutex;
	HANDLE m_hMapFile;
	HWND m_hWnd;
};

#endif // RUNONCE_H
#include "RunOnce.h"

int RandInt(int min, int max);

RunOnce::RunOnce(HWND hWnd)
{
	m_pszMutexPathFile = new TCHAR[MAX_PATH];
	m_pszMutexName = new TCHAR[MAX_PATH];

	SecureZeroMemory(m_pszMutexPathFile, MAX_PATH);
	SecureZeroMemory(m_pszMutexName, MAX_PATH);

	m_hMutexFile = 0;
	m_hMutex = 0;
	m_hMapFile = 0;
	m_hWnd = hWnd;
}

RunOnce::~RunOnce()
{
	if (m_hMutex)
	{
		ReleaseMutex(m_hMutex);
		CloseHandle(m_hMutex);
	}

	if (m_hMutexFile)
	{
		CloseHandle(m_hMutexFile);
		DeleteFile(m_pszMutexPathFile);
	}

	if (m_hMapFile)
		CloseHandle(m_hMapFile);

	delete [] m_pszMutexPathFile;
	delete [] m_pszMutexName;
}

BOOL RunOnce::GetMutexPathFile(TCHAR* lpFile)
{
	SecureZeroMemory(m_pszMutexPathFile, MAX_PATH);

	// Common directory is C:\Documents and Settings\User\Application Data
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, m_pszMutexPathFile)))
		return FALSE;

	if (lpFile)
	{
		if (PathAppend(m_pszMutexPathFile, lpFile) == FALSE)
			return FALSE;
	}

	return TRUE;
}

BOOL RunOnce::GetMutexName()
{
	_tifstream inMutex;

	SecureZeroMemory(m_pszMutexName, MAX_PATH);

	if (!PathFileExists(m_pszMutexPathFile))
		return FALSE;

	inMutex.open(m_pszMutexPathFile, ios::binary);
	if (inMutex.fail())
		return FALSE;

	inMutex.read(m_pszMutexName, MAX_PATH);
	inMutex.close();

	return TRUE;
}

BOOL RunOnce::MutexNameExists()
{
	if ((m_hMutex = CreateMutex(NULL, TRUE, m_pszMutexName)) == 0)
		return FALSE;
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		return TRUE;

	return FALSE;
}

BOOL RunOnce::GenMutexName()
{
	SecureZeroMemory(m_pszMutexName, MAX_PATH);

	_tcscpy_s(m_pszMutexName, 8, TEXT("Global\\\0"));
	for (int i = 7; i < MAX_PATH - 1; i++)
		m_pszMutexName[i] = static_cast<char> (RandInt(93, 126));
	m_pszMutexName[MAX_PATH - 1] = '\0';

	return TRUE;
}

BOOL RunOnce::SetMutexName()
{
	DWORD dwNumberOfBytesWritten = 0;

	m_hMutexFile = CreateFile(
		m_pszMutexPathFile,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN,
		NULL);

	if (GetLastError() != 0) // ERROR_SUCCESS
	{
		if (m_hMutexFile == INVALID_HANDLE_VALUE)
			return FALSE;
	}

	if (!WriteFile(m_hMutexFile, m_pszMutexName, MAX_PATH, &dwNumberOfBytesWritten, NULL))
		return FALSE;

	return TRUE;
}

BOOL RunOnce::SetPageWindow()
{
	PVOID pBuffer = 0;

	m_pszMutexName[MAX_PATH - 2]++;
	
	m_hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(HWND),
		m_pszMutexName);

	if (m_hMapFile == NULL)
		return FALSE;

	pBuffer = MapViewOfFile(
		m_hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(HWND));

	if (pBuffer == NULL)
		CloseHandle(m_hMapFile);

	CopyMemory(pBuffer, &m_hWnd, sizeof(HWND));

	UnmapViewOfFile(pBuffer);

	return TRUE;
}

BOOL RunOnce::BringToFocus()
{
	HANDLE hMapFile = 0;
	PVOID pBuffer = 0;

	m_pszMutexName[MAX_PATH - 2] = m_pszMutexName[MAX_PATH - 2] + 1;

	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		m_pszMutexName);

	if (hMapFile == NULL)
		return FALSE;

	pBuffer = MapViewOfFile(
		hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(HWND));

	if (pBuffer == NULL)
		CloseHandle(hMapFile);

	CopyMemory(&m_hWnd, pBuffer, sizeof(HWND));

	if (!SetForegroundWindow(reinterpret_cast<HWND> (m_hWnd)))
		return FALSE;

	return TRUE;
}

BOOL RunOnce::PerformCheck(TCHAR* lpFile)
{
	if (!GetMutexPathFile(lpFile))
		return FALSE;

	if (!GetMutexName())
	{
		if (!GenMutexName())
			return FALSE;
	}

	if (MutexNameExists())
	{
		if (!BringToFocus())
			return FALSE;
		return FALSE;
	}
	else
	{
		if (!SetMutexName())
			return FALSE;
		if (!SetPageWindow())
			return FALSE;
	}

	return TRUE;
}

int RandInt(int min, int max)
{
	return min + rand() % (max - min + 1);
}
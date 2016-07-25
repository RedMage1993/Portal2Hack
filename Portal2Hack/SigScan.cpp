#include "SigScan.h"

SigScan::SigScan(TCHAR* exeName, TCHAR* modName)
{
	m_exeName = exeName;

	if (!_tcscmp(modName, TEXT("")))
		m_modName = m_exeName;
	else
		m_modName = modName;

	m_processId = 0;
	m_process = 0;
	m_baseAddr = 0;
	m_baseSize = 0;
}

SigScan::~SigScan()
{
	CloseHandle(m_process);
}

BOOL SigScan::FindProcessId()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe32;

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(snapshot, &pe32))
		return FALSE;
	
	do
	{
	    if (!_tcscmp(m_exeName, pe32.szExeFile))
		{
			m_processId = pe32.th32ProcessID;
			break;
		}
	} while (Process32Next(snapshot, &pe32));

	CloseHandle(snapshot);

	return m_processId != 0;
}

void SigScan::SetProcessId(DWORD processId)
{
	m_processId = processId;
}

BOOL SigScan::OpenProcess()
{
	m_process = ::OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE |
		PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, m_processId);

	return m_process != 0;
}

BOOL SigScan::FindModInfo()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_processId);
	MODULEENTRY32 me32;

	me32.dwSize = sizeof(MODULEENTRY32);
	
	if (!Module32First(snapshot, &me32))
		return FALSE;
	
	do
	{
	    if (!_tcscmp(m_modName, me32.szModule))
		{
			m_baseAddr = reinterpret_cast<UINT_PTR> (me32.modBaseAddr);
			m_baseSize = me32.modBaseSize;
			break;
		}
	} while (Module32Next(snapshot, &me32));

	CloseHandle(snapshot);

	return m_baseAddr != 0 || m_baseSize != 0;
}

BOOL SigScan::Scan(TCHAR* sig, UINT_PTR* &results, DWORD size, DWORD& count)
{
	CONST TCHAR HEX[] = {'0', '1', '2', '3', '4', '5',
		'6', '7', '8', '9', 'A', 'B',
		'C', 'D', 'E', 'F'};

	MEMORY_BASIC_INFORMATION mbi;
	DWORD prevProtect = 0;
	DWORD temp;
	BOOL modified;
	UINT_PTR address = m_baseAddr;
	BYTE* data;
	size_t sigLen = _tcslen(sig);
	size_t sigByte = 0;
	size_t offset = 0;
	TCHAR* ptrStart = _tcschr(sig, 'X');

	if (sigLen % 2 != 0)
		return FALSE;

	if (ptrStart != 0)
		offset = (ptrStart - sig) / 2;

	count = 0;

	do
	{
		if (!VirtualQueryEx (m_process, reinterpret_cast<LPCVOID> (address),
			&mbi, sizeof (MEMORY_BASIC_INFORMATION)))
			return FALSE;

		if (mbi.State == MEM_COMMIT)
		{
			modified = FALSE;
			if ((mbi.Protect & PAGE_GUARD) == PAGE_GUARD ||
				(mbi.Protect & PAGE_NOACCESS) == PAGE_NOACCESS)
			{
				if (!VirtualProtectEx (m_process, reinterpret_cast<LPVOID> (address),
					mbi.RegionSize, PAGE_READONLY, &prevProtect))
					return FALSE;

				modified = TRUE;
			}

			data = new BYTE[mbi.RegionSize];
			if (!ReadProcessMemory (m_process, reinterpret_cast<LPCVOID> (address),
				reinterpret_cast<LPVOID> (data), mbi.RegionSize, NULL))
				return FALSE;

			for (size_t dataByte = 0; dataByte < mbi.RegionSize - sigLen;)
			{
				sigByte = 0;
				for (size_t matchLen = 0; sigByte < sigLen - 1; matchLen++)
				{
					if (sig[sigByte] == 'X' || sig[sigByte] == '!' ||
						sig[sigByte + 1] == 'X' || sig[sigByte + 1] == '!')
					{
						sigByte += 2;
						continue;
					}

					if (sig[sigByte] != HEX[data[dataByte + matchLen] >> 4]
						|| sig[sigByte + 1] != HEX[(((data[dataByte + matchLen] >> 4) << 4)
						^ data[dataByte + matchLen])])
						break;

					sigByte += 2;
				}

				if (sigByte == sigLen)
				{
					if (offset != 0)
					{
						if (!ReadProcessMemory(m_process, reinterpret_cast<LPCVOID> (address + dataByte + offset),
							reinterpret_cast<LPVOID> (&results[count]), sizeof(results[count]), NULL))
							return FALSE;
					}
					else
					{
						results[count] = address + dataByte;
					}


					if (++count == size)
						break;

					dataByte += sigLen;
				}
				else
				{
					dataByte++;
				}
			}

			delete [] data;

			if (modified == TRUE)
			{
				if (!VirtualProtectEx (m_process, reinterpret_cast<LPVOID> (address),
					mbi.RegionSize, prevProtect, &temp))
					return FALSE;
			}
		}

		address += mbi.RegionSize;
	} while (address < m_baseSize + m_baseAddr && count < size);

	return TRUE;
}

BOOL SigScan::Scan(TCHAR* sig, UINT_PTR* result)
{
	CONST TCHAR HEX[] = {'0', '1', '2', '3', '4', '5',
		'6', '7', '8', '9', 'A', 'B',
		'C', 'D', 'E', 'F'};

	MEMORY_BASIC_INFORMATION mbi;
	DWORD prevProtect = 0;
	DWORD temp;
	BOOL modified;
	UINT_PTR address = m_baseAddr;
	BYTE* data;
	size_t sigLen = _tcslen(sig);
	size_t sigByte = 0;
	size_t offset = 0;
	TCHAR* ptrStart = _tcschr(sig, 'X');

	if (sigLen % 2 != 0)
		return FALSE;

	if (ptrStart != 0)
		offset = (ptrStart - sig) / 2;

	*result = 0;

	do
	{
		if (!VirtualQueryEx (m_process, reinterpret_cast<LPCVOID> (address),
			&mbi, sizeof (MEMORY_BASIC_INFORMATION)))
			return FALSE;

		if (mbi.State == MEM_COMMIT)
		{
			modified = FALSE;
			if ((mbi.Protect & PAGE_GUARD) == PAGE_GUARD ||
				(mbi.Protect & PAGE_NOACCESS) == PAGE_NOACCESS)
			{
				if (!VirtualProtectEx (m_process, reinterpret_cast<LPVOID> (address),
					mbi.RegionSize, PAGE_READONLY, &prevProtect))
					return FALSE;

				modified = TRUE;
			}

			data = new BYTE[mbi.RegionSize];
			if (!ReadProcessMemory (m_process, reinterpret_cast<LPCVOID> (address),
				reinterpret_cast<LPVOID> (data), mbi.RegionSize, NULL))
				return FALSE;

			for (size_t dataByte = 0; dataByte < mbi.RegionSize - sigLen;)
			{
				sigByte = 0;
				for (size_t matchLen = 0; sigByte < sigLen - 1; matchLen++)
				{
					if (sig[sigByte] == 'X' || sig[sigByte] == '!' ||
						sig[sigByte + 1] == 'X' || sig[sigByte + 1] == '!')
					{
						sigByte += 2;
						continue;
					}

					if (sig[sigByte] != HEX[data[dataByte + matchLen] >> 4]
						|| sig[sigByte + 1] != HEX[(((data[dataByte + matchLen] >> 4) << 4)
						^ data[dataByte + matchLen])])
						break;

					sigByte += 2;
				}

				if (sigByte == sigLen)
				{
					if (offset != 0)
					{
						if (!ReadProcessMemory(m_process, reinterpret_cast<LPCVOID> (address + dataByte + offset),
							reinterpret_cast<LPVOID> (result), sizeof(*result), NULL))
							return FALSE;
					}
					else
					{
						*result = address + dataByte;
					}

					delete [] data;
					return TRUE;
				}
				else
				{
					dataByte++;
				}
			}

			delete [] data;

			if (modified == TRUE)
			{
				if (!VirtualProtectEx (m_process, reinterpret_cast<LPVOID> (address),
					mbi.RegionSize, prevProtect, &temp))
					return FALSE;
			}
		}

		address += mbi.RegionSize;
	} while (address < m_baseSize + m_baseAddr);

	return TRUE;
}

DWORD SigScan::GetProcessId() const
{
	return m_processId;
}

UINT_PTR SigScan::GetBaseAddr() const
{
	return m_baseAddr;
}

HANDLE SigScan::GetProcess() const
{
	return m_process;
}

DWORD SigScan::GetBaseSize() const
{
	return m_baseSize;
}
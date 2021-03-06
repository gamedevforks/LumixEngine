#include "debug/debug.h"
#include "core/string.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <mapi.h>


#pragma comment(lib, "DbgHelp.lib")

namespace Lumix
{


namespace Debug
{


void debugOutput(const char* message)
{
	OutputDebugString(message);
}


void debugBreak()
{
	DebugBreak();
}


} // namespace Debug


BOOL SendFile(LPCSTR lpszSubject,
			  LPCSTR lpszTo,
			  LPCSTR lpszName,
			  LPCSTR lpszText,
			  LPCSTR lpszFullFileName)
{
	HINSTANCE hMAPI = ::LoadLibrary("mapi32.dll");
	if (!hMAPI)
		return FALSE;
	LPMAPISENDMAIL lpfnMAPISendMail =
		(LPMAPISENDMAIL)::GetProcAddress(hMAPI, "MAPISendMail");

	char szDrive[_MAX_DRIVE] = {0};
	char szDir[_MAX_DIR] = {0};
	char szName[_MAX_FNAME] = {0};
	char szExt[_MAX_EXT] = {0};
	_splitpath_s(lpszFullFileName, szDrive, szDir, szName, szExt);

	char szFileName[MAX_PATH] = {0};
	strcat_s(szFileName, szName);
	strcat_s(szFileName, szExt);

	char szFullFileName[MAX_PATH] = {0};
	strcat_s(szFullFileName, lpszFullFileName);

	MapiFileDesc MAPIfile = {0};
	ZeroMemory(&MAPIfile, sizeof(MapiFileDesc));
	MAPIfile.nPosition = 0xFFFFFFFF;
	MAPIfile.lpszPathName = szFullFileName;
	MAPIfile.lpszFileName = szFileName;

	char szTo[MAX_PATH] = {0};
	strcat_s(szTo, lpszTo);

	char szNameTo[MAX_PATH] = {0};
	strcat_s(szNameTo, lpszName);

	MapiRecipDesc recipient = {0};
	recipient.ulRecipClass = MAPI_TO;
	recipient.lpszAddress = szTo;
	recipient.lpszName = szNameTo;

	char szSubject[MAX_PATH] = {0};
	strcat_s(szSubject, lpszSubject);

	char szText[MAX_PATH] = {0};
	strcat_s(szText, lpszText);

	MapiMessage MAPImsg = {0};
	MAPImsg.lpszSubject = szSubject;
	MAPImsg.lpRecips = &recipient;
	MAPImsg.nRecipCount = 1;
	MAPImsg.lpszNoteText = szText;
	MAPImsg.nFileCount = 1;
	MAPImsg.lpFiles = &MAPIfile;

	ULONG nSent = lpfnMAPISendMail(0, 0, &MAPImsg, 0, 0);

	FreeLibrary(hMAPI);
	return (nSent == SUCCESS_SUCCESS || nSent == MAPI_E_USER_ABORT);
}


static LONG WINAPI unhandledExceptionHandler(LPEXCEPTION_POINTERS info)
{
	HANDLE process = GetCurrentProcess();
	DWORD process_id = GetProcessId(process);
	HANDLE file = CreateFile("minidump.dmp",
							 GENERIC_WRITE,
							 0,
							 nullptr,
							 CREATE_ALWAYS,
							 FILE_ATTRIBUTE_NORMAL,
							 nullptr);
	MINIDUMP_TYPE minidump_type = (MINIDUMP_TYPE)(
		/*MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo |*/ MiniDumpFilterMemory |
		MiniDumpWithHandleData | MiniDumpWithThreadInfo |
		MiniDumpWithUnloadedModules);

	MINIDUMP_EXCEPTION_INFORMATION minidump_exception_info;
	minidump_exception_info.ThreadId = GetCurrentThreadId();
	minidump_exception_info.ExceptionPointers = info;
	minidump_exception_info.ClientPointers = FALSE;

	MiniDumpWriteDump(process,
					  process_id,
					  file,
					  minidump_type,
					  info ? &minidump_exception_info : nullptr,
					  nullptr,
					  nullptr);
	CloseHandle(file);

	SendFile("Lumix Studio crash",
			 "SMTP:mikulas.florek@gamedev.sk",
			 "Lumix Studio",
			 "Lumix Studio crashed, minidump attached",
			 "minidump.dmp");

	minidump_type =
		(MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo |
						MiniDumpFilterMemory | MiniDumpWithHandleData |
						MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
	file = CreateFile("fulldump.dmp",
					  GENERIC_WRITE,
					  0,
					  nullptr,
					  CREATE_ALWAYS,
					  FILE_ATTRIBUTE_NORMAL,
					  nullptr);
	MiniDumpWriteDump(process,
					  process_id,
					  file,
					  minidump_type,
					  info ? &minidump_exception_info : nullptr,
					  nullptr,
					  nullptr);
	CloseHandle(file);

	return EXCEPTION_CONTINUE_SEARCH;
}


void installUnhandledExceptionHandler()
{
	SetUnhandledExceptionFilter(unhandledExceptionHandler);
}


} // namespace Lumix
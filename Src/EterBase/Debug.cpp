#include "StdAfx.h"
#include "../UserInterface/Locale_inc.h"

#include <ctime>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")
#include "Debug.h"
#include "Singleton.h"
#include "Timer.h"
#ifdef ENABLE_SYSERR_BY_PID
#	include <fmt/fmt.h>
#endif

const uint32_t DEBUG_STRING_MAX_LEN = 1024;

static int isLogFile = false;
HWND g_PopupHwnd = nullptr;

class CLogFile : public CSingleton<CLogFile>
{
public:
	CLogFile() = default;

	~CLogFile() override = default;

	void Initialize() { m_fPtr.open("log.txt", "w"); }

	void Write(const char * c_pszMsg) const
	{
		if (!m_fPtr)
			return;

		time_t ct = time(nullptr);
		struct tm ctm = *localtime(&ct);

		fprintf(m_fPtr.get(), "%02d%02d %02d:%02d:%05u :: %s", ctm.tm_mon + 1, ctm.tm_mday, ctm.tm_hour, ctm.tm_min,
				ELTimer_GetMSec() % 60000, c_pszMsg);

		fflush(m_fPtr.get());
	}

protected:
	msl::file_ptr m_fPtr;
};

static CLogFile gs_logfile;
static uint32_t gs_uLevel = 0;

void SetLogLevel(uint32_t uLevel)
{
	gs_uLevel = uLevel;
}

void Log(uint32_t uLevel, const char * c_szMsg)
{
	if (uLevel >= gs_uLevel)
		Trace(c_szMsg);
}

void Logn(uint32_t uLevel, const char * c_szMsg)
{
	if (uLevel >= gs_uLevel)
		Tracen(c_szMsg);
}

void Logf(uint32_t uLevel, const char * c_szFormat, ...)
{
	if (uLevel < gs_uLevel)
		return;

	char szBuf[DEBUG_STRING_MAX_LEN + 1];

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);
	va_end(args);
#ifdef _DEBUG
	OutputDebugString(szBuf);
	fputs(szBuf, stdout);
#endif

	if (isLogFile)
		LogFile(szBuf);
}

void Lognf(uint32_t uLevel, const char * c_szFormat, ...)
{
	if (uLevel < gs_uLevel)
		return;

	va_list args;
	va_start(args, c_szFormat);

	char szBuf[DEBUG_STRING_MAX_LEN + 2];
	int len = _vsnprintf(szBuf, sizeof(szBuf) - 1, c_szFormat, args);

	if (len > 0)
	{
		szBuf[len] = '\n';
		szBuf[len + 1] = '\0';
	}
	va_end(args);
#ifdef _DEBUG
	OutputDebugString(szBuf);
	puts(szBuf);
#endif

	if (isLogFile)
		LogFile(szBuf);
}


void Trace(const char * c_szMsg)
{
#ifdef _DEBUG
	OutputDebugString(c_szMsg);
	printf("%s", c_szMsg);
#endif

	if (isLogFile)
		LogFile(c_szMsg);
}

void Tracen(const char * c_szMsg)
{
#ifdef _DEBUG
	char szBuf[DEBUG_STRING_MAX_LEN + 1];
	_snprintf(szBuf, sizeof(szBuf), "%s\n", c_szMsg);
	OutputDebugString(szBuf);
	puts(c_szMsg);

	if (isLogFile)
		LogFile(szBuf);

	puts(c_szMsg);
	putc('\n', stdout);
#else
	if (isLogFile)
	{
		LogFile(c_szMsg);
		LogFile("\n");
	}
#endif
}

void Tracenf(const char * c_szFormat, ...)
{
#ifndef NDEBUG // disabled on release
	va_list args;
	va_start(args, c_szFormat);

	char szBuf[DEBUG_STRING_MAX_LEN + 2];
	int len = _vsnprintf(szBuf, sizeof(szBuf) - 1, c_szFormat, args);

	if (len > 0)
	{
		szBuf[len] = '\n';
		szBuf[len + 1] = '\0';
	}
	va_end(args);
#	ifdef _DEBUG
	OutputDebugString(szBuf);
	printf("%s", szBuf);
#	endif

	if (isLogFile)
		LogFile(szBuf);
#endif
}

void Tracef(const char * c_szFormat, ...)
{
	char szBuf[DEBUG_STRING_MAX_LEN + 1];

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);
	va_end(args);

#ifdef _DEBUG
	OutputDebugString(szBuf);
	fputs(szBuf, stdout);
#endif

	if (isLogFile)
		LogFile(szBuf);
}

void TraceError(const char * c_szFormat, ...)
{
	char szMessage[DEBUG_STRING_MAX_LEN + 1] = {};

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, c_szFormat, args);
	va_end(args);

	time_t ct = time(nullptr);
	struct tm ctm = *localtime(&ct);

	fprintf(
		stderr,
		"%02d%02d %02d:%02d:%05u :: %s\n",
		ctm.tm_mon + 1,
		ctm.tm_mday,
		ctm.tm_hour,
		ctm.tm_min,
		ELTimer_GetMSec() % 60000,
		szMessage);
	fflush(stderr);

#ifdef _DEBUG
	char szDebug[DEBUG_STRING_MAX_LEN + 16] = {};
	_snprintf_s(szDebug, sizeof(szDebug), _TRUNCATE, "SYSERR: %s\n", szMessage);
	OutputDebugStringA(szDebug);
	fputs(szDebug, stdout);
#endif

	if (isLogFile)
	{
		char szLog[DEBUG_STRING_MAX_LEN + 16] = {};
		_snprintf_s(szLog, sizeof(szLog), _TRUNCATE, "SYSERR: %s\n", szMessage);
		LogFile(szLog);
	}
}

void TraceErrorWithoutEnter(const char * c_szFormat, ...)
{
	char szMessage[DEBUG_STRING_MAX_LEN + 1] = {};

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, c_szFormat, args);
	va_end(args);

	time_t ct = time(nullptr);
	struct tm ctm = *localtime(&ct);

	fprintf(
		stderr,
		"%02d%02d %02d:%02d:%05u :: %s",
		ctm.tm_mon + 1,
		ctm.tm_mday,
		ctm.tm_hour,
		ctm.tm_min,
		ELTimer_GetMSec() % 60000,
		szMessage);
	fflush(stderr);

#ifdef _DEBUG
	OutputDebugStringA(szMessage);
	fputs(szMessage, stdout);
#endif

	if (isLogFile)
		LogFile(szMessage);
}

void LogBoxf(const char * c_szFormat, ...)
{
	va_list args;
	va_start(args, c_szFormat);

	char szBuf[2048];
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);

	LogBox(szBuf);
}

void LogBox(const char * c_szMsg, const char * c_szCaption, HWND hWnd)
{
	if (!hWnd)
		hWnd = g_PopupHwnd;

	MessageBox(hWnd, c_szMsg, c_szCaption ? c_szCaption : "LOG", MB_OK);
	Tracen(c_szMsg);
}

void LogFile(const char * c_szMsg)
{
	CLogFile::Instance().Write(c_szMsg);
}

void LogFilef(const char * c_szMessage, ...)
{
	va_list args;
	va_start(args, c_szMessage);
	char szBuf[DEBUG_STRING_MAX_LEN + 1];
	_vsnprintf(szBuf, sizeof(szBuf), c_szMessage, args);

	CLogFile::Instance().Write(szBuf);
}

void OpenLogFile(bool bUseLogFIle)
{
	CreateDirectoryA("./syserr", nullptr);

	time_t rawtime;
	time(&rawtime);
	struct tm * timeinfo = localtime(&rawtime);

	char szSyserrPath[128] = {};
	strftime(
		szSyserrPath,
		sizeof(szSyserrPath),
		"./syserr/%d-%m-%Y-%H-%M-%S-syserr.txt",
		timeinfo);

	if (freopen(szSyserrPath, "w", stderr) == nullptr)
		OutputDebugStringA("OpenLogFile: could not open timestamped syserr file.\n");

	if (bUseLogFIle)
	{
		isLogFile = true;
		CLogFile::Instance().Initialize();
	}
}

static bool gs_bCrashSymbolsInitialized = false;

static const char * GetExceptionName(DWORD dwCode)
{
	switch (dwCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:   return "ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_BREAKPOINT:              return "BREAKPOINT";
	case EXCEPTION_DATATYPE_MISALIGNMENT:   return "DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:      return "FLOAT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_OVERFLOW:            return "FLOAT_OVERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION:     return "ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:           return "IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:      return "INTEGER_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:            return "INTEGER_OVERFLOW";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:return "NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_PRIV_INSTRUCTION:        return "PRIVILEGED_INSTRUCTION";
	case EXCEPTION_STACK_OVERFLOW:          return "STACK_OVERFLOW";
	case 0xC0000374:                        return "HEAP_CORRUPTION";
	case 0xC0000409:                        return "STACK_BUFFER_OVERRUN_OR_FAST_FAIL";
	default:                                return "UNKNOWN_EXCEPTION";
	}
}

static void InitializeCrashSymbols()
{
	if (gs_bCrashSymbolsInitialized)
		return;

	gs_bCrashSymbolsInitialized = true;

	SymSetOptions(
		SYMOPT_LOAD_LINES |
		SYMOPT_UNDNAME |
		SYMOPT_DEFERRED_LOADS |
		SYMOPT_FAIL_CRITICAL_ERRORS);

	char szExePath[MAX_PATH] = {};
	GetModuleFileNameA(nullptr, szExePath, MAX_PATH);

	char * pLastSlash = strrchr(szExePath, '\\');
	if (pLastSlash)
		*pLastSlash = '\0';

	char szSearchPath[MAX_PATH * 3] = {};
	_snprintf_s(
		szSearchPath,
		sizeof(szSearchPath),
		_TRUNCATE,
		"%s;%s\\symbols;.",
		szExePath,
		szExePath);

	if (!SymInitialize(GetCurrentProcess(), szSearchPath, TRUE))
	{
		fprintf(
			stderr,
			"CRASH LOGGER WARNING: SymInitialize failed. Win32Error=%lu\n",
			GetLastError());
		fflush(stderr);
	}
}

static void PrintSymbolizedAddress(DWORD64 qwAddress, unsigned int uFrame)
{
	if (!qwAddress)
		return;

	HANDLE hProcess = GetCurrentProcess();

	alignas(SYMBOL_INFO) char szSymbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME + 1] = {};
	PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(szSymbolBuffer);
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	DWORD64 qwSymbolDisplacement = 0;

	IMAGEHLP_LINE64 kLine = {};
	kLine.SizeOfStruct = sizeof(kLine);
	DWORD dwLineDisplacement = 0;

	IMAGEHLP_MODULE64 kModule = {};
	kModule.SizeOfStruct = sizeof(kModule);

	const BOOL bHasSymbol =
		SymFromAddr(hProcess, qwAddress, &qwSymbolDisplacement, pSymbol);

	const BOOL bHasLine =
		SymGetLineFromAddr64(hProcess, qwAddress, &dwLineDisplacement, &kLine);

	const BOOL bHasModule =
		SymGetModuleInfo64(hProcess, qwAddress, &kModule);

	const char * c_szModule =
		(bHasModule && kModule.ModuleName[0])
			? kModule.ModuleName
			: "<unknown-module>";

	if (bHasSymbol)
	{
		fprintf(
			stderr,
			"#%02u %s!%s + 0x%I64X [0x%p]\n",
			uFrame,
			c_szModule,
			pSymbol->Name,
			static_cast<unsigned __int64>(qwSymbolDisplacement),
			reinterpret_cast<void *>(static_cast<uintptr_t>(qwAddress)));
	}
	else
	{
		fprintf(
			stderr,
			"#%02u %s!<unknown-symbol> [0x%p]\n",
			uFrame,
			c_szModule,
			reinterpret_cast<void *>(static_cast<uintptr_t>(qwAddress)));
	}

	if (bHasLine && kLine.FileName)
	{
		fprintf(
			stderr,
			"     SOURCE: %s:%lu\n",
			kLine.FileName,
			kLine.LineNumber);
	}
	else
	{
		fprintf(
			stderr,
			"     SOURCE: <PDB/source line unavailable>\n");
	}

	fflush(stderr);
}

static void PrintCurrentStack()
{
	InitializeCrashSymbols();

	void * apFrames[64] = {};
	const USHORT usFrameCount =
		CaptureStackBackTrace(2, 64, apFrames, nullptr);

	fprintf(stderr, "\nSTACK TRACE\n");

	for (USHORT i = 0; i < usFrameCount; ++i)
		PrintSymbolizedAddress(reinterpret_cast<DWORD64>(apFrames[i]), i);

	fflush(stderr);
}

static void PrintExceptionStack(EXCEPTION_POINTERS * pExceptionInfo)
{
	if (!pExceptionInfo || !pExceptionInfo->ContextRecord)
		return;

	InitializeCrashSymbols();

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	CONTEXT kContext = *pExceptionInfo->ContextRecord;
	STACKFRAME64 kFrame = {};
	DWORD dwMachineType = 0;

#if defined(_M_X64)
	dwMachineType = IMAGE_FILE_MACHINE_AMD64;

	kFrame.AddrPC.Offset = kContext.Rip;
	kFrame.AddrPC.Mode = AddrModeFlat;
	kFrame.AddrFrame.Offset = kContext.Rbp;
	kFrame.AddrFrame.Mode = AddrModeFlat;
	kFrame.AddrStack.Offset = kContext.Rsp;
	kFrame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IX86)
	dwMachineType = IMAGE_FILE_MACHINE_I386;

	kFrame.AddrPC.Offset = kContext.Eip;
	kFrame.AddrPC.Mode = AddrModeFlat;
	kFrame.AddrFrame.Offset = kContext.Ebp;
	kFrame.AddrFrame.Mode = AddrModeFlat;
	kFrame.AddrStack.Offset = kContext.Esp;
	kFrame.AddrStack.Mode = AddrModeFlat;
#else
#	error Unsupported Windows architecture
#endif

	fprintf(stderr, "\nSTACK TRACE\n");
	PrintSymbolizedAddress(kFrame.AddrPC.Offset, 0);

	for (unsigned int i = 1; i < 64; ++i)
	{
		if (!StackWalk64(
				dwMachineType,
				hProcess,
				hThread,
				&kFrame,
				&kContext,
				nullptr,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				nullptr))
		{
			break;
		}

		if (!kFrame.AddrPC.Offset)
			break;

		PrintSymbolizedAddress(kFrame.AddrPC.Offset, i);
	}

	fflush(stderr);
}

void TraceControlledExit(const char * c_szReason, const char * c_szFile, int iLine)
{
	fprintf(
		stderr,
		"\n============================================================\n"
		"CONTROLLED CLIENT EXIT\n"
		"Reason : %s\n"
		"Source : %s:%d\n"
		"Thread : %lu\n"
		"============================================================\n",
		c_szReason ? c_szReason : "<unknown>",
		c_szFile ? c_szFile : "<unknown>",
		iLine,
		GetCurrentThreadId());

	PrintCurrentStack();

	fprintf(
		stderr,
		"============================================================\n");
	fflush(stderr);
}

void TraceControlledExitf(const char * c_szFile, int iLine, const char * c_szFormat, ...)
{
	char szReason[1024] = {};

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf_s(szReason, sizeof(szReason), _TRUNCATE, c_szFormat, args);
	va_end(args);

	TraceControlledExit(szReason, c_szFile, iLine);
}

static LONG WINAPI EterUnhandledExceptionFilter(EXCEPTION_POINTERS * pExceptionInfo)
{
	DWORD dwCode = 0;
	void * pAddress = nullptr;

	if (pExceptionInfo && pExceptionInfo->ExceptionRecord)
	{
		dwCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
		pAddress = pExceptionInfo->ExceptionRecord->ExceptionAddress;
	}

	fprintf(
		stderr,
		"\n============================================================\n"
		"FATAL CLIENT CRASH\n"
		"Exception : 0x%08lX (%s)\n"
		"Address   : %p\n"
		"Thread    : %lu\n",
		dwCode,
		GetExceptionName(dwCode),
		pAddress,
		GetCurrentThreadId());

	if (pExceptionInfo &&
		pExceptionInfo->ExceptionRecord &&
		dwCode == EXCEPTION_ACCESS_VIOLATION &&
		pExceptionInfo->ExceptionRecord->NumberParameters >= 2)
	{
		const ULONG_PTR ulOperation =
			pExceptionInfo->ExceptionRecord->ExceptionInformation[0];

		const ULONG_PTR ulBadAddress =
			pExceptionInfo->ExceptionRecord->ExceptionInformation[1];

		const char * c_szOperation = "UNKNOWN";

		if (ulOperation == 0)
			c_szOperation = "READ";
		else if (ulOperation == 1)
			c_szOperation = "WRITE";
		else if (ulOperation == 8)
			c_szOperation = "EXECUTE";

		fprintf(
			stderr,
			"Problem   : Invalid %s at address %p\n",
			c_szOperation,
			reinterpret_cast<void *>(ulBadAddress));
	}

	fprintf(
		stderr,
		"============================================================\n"
		"CRASH LOCATION\n");

	PrintSymbolizedAddress(
		reinterpret_cast<DWORD64>(pAddress),
		0);

	PrintExceptionStack(pExceptionInfo);

	fprintf(
		stderr,
		"============================================================\n");
	fflush(stderr);

	return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler()
{
	static bool s_bRuntimeHooksInstalled = false;

	InitializeCrashSymbols();

	if (!s_bRuntimeHooksInstalled)
	{
		s_bRuntimeHooksInstalled = true;

		std::set_terminate([]()
		{
			TraceControlledExit(
				"std::terminate() called",
				__FILE__,
				__LINE__);

			TerminateProcess(
				GetCurrentProcess(),
				0xE0000001);
		});

		fprintf(
			stderr,
			"CRASH LOGGER INITIALIZED | timestamped-syserr=ON | symbols=ON | source-lines=ON | builds=ALL\n");
		fflush(stderr);
	}

	SetErrorMode(
		SEM_FAILCRITICALERRORS |
		SEM_NOGPFAULTERRORBOX);

	SetUnhandledExceptionFilter(EterUnhandledExceptionFilter);
}

void OpenConsoleWindow()
{
	AllocConsole();

	freopen("CONOUT$", "a", stdout);
	freopen("CONIN$", "r", stdin);
}

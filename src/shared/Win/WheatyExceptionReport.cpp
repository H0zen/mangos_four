//==========================================
// Matt Pietrek
// MSDN Magazine, 2002
// FILE: WheatyExceptionReport.CPP
//==========================================
#define WIN32_LEAN_AND_MEAN
#pragma warning(disable:4996)
#pragma warning(disable:4312)
#pragma warning(disable:4311)
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <tchar.h>
#include <algorithm>
#define _NO_CVCONST_H
#include <dbghelp.h>
#include "WheatyExceptionReport.h"
#include "GitRevision.h"
#define CrashFolder _T("Crashes")
#pragma comment(linker, "/defaultlib:dbghelp.lib")

inline LPTSTR ErrorMessage(DWORD dw)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL);
    return (LPTSTR)lpMsgBuf;
}

//============================== Global Variables =============================

//
// Declare the static variables of the WheatyExceptionReport class
//
TCHAR WheatyExceptionReport::m_szLogFileName[MAX_PATH];
TCHAR WheatyExceptionReport::m_szDumpFileName[MAX_PATH];
bool  WheatyExceptionReport::m_dumpWritten;
bool  WheatyExceptionReport::m_dumpHelperRunning;
LPTOP_LEVEL_EXCEPTION_FILTER WheatyExceptionReport::m_previousFilter;
HANDLE WheatyExceptionReport::m_hReportFile;
HANDLE WheatyExceptionReport::m_hProcess;

// Declare global instance of class
WheatyExceptionReport g_WheatyExceptionReport;

//============================== Class Methods =============================

WheatyExceptionReport::WheatyExceptionReport()              // Constructor
{
    // Install the unhandled exception filter function
    m_previousFilter = SetUnhandledExceptionFilter(WheatyUnhandledExceptionFilter);
    m_hProcess = GetCurrentProcess();
}

//============
// Destructor
//============
WheatyExceptionReport::~WheatyExceptionReport()
{
    if (m_previousFilter)
    {
        SetUnhandledExceptionFilter(m_previousFilter);
    }
}

//===========================================================
// Writes a minidump beside the text report.
//
// The text report can only describe what its symbol walker manages to decode,
// and on optimised x64 that is not much: it prints parameter names without
// values, because the walker computes stack addresses the way a 2002 x86 sample
// did. A dump hands the same question to WinDbg, which has the real unwind
// machinery and the variable-range records the compiler emitted. That is a far
// better position, not a guarantee: a value the optimiser never stored anywhere
// is not in the dump either.
//===========================================================
bool WheatyExceptionReport::WriteMiniDumpWorker(PEXCEPTION_POINTERS pExceptionInfo, DWORD faultingThreadId)
{
    HANDLE hDumpFile = CreateFile(m_szDumpFileName,
                                  GENERIC_WRITE,
                                  0,
                                  0,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  0);

    // CreateFile signals failure with INVALID_HANDLE_VALUE, which is truthy - a
    // plain `if (handle)` would happily write into a dead handle.
    if (hDumpFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei;
    // Passed in, never GetCurrentThreadId(): this runs on a helper thread, so asking
    // here would name the helper and the dump would open on the wrong thread.
    mei.ThreadId = faultingThreadId;
    mei.ExceptionPointers = pExceptionInfo;
    mei.ClientPointers = FALSE;                             // same process, so the pointer is ours to deref

    // Triage set, then two progressively smaller fallbacks. MiniDumpWriteDump fails
    // the whole call rather than ignoring a flag it does not understand, and a single
    // unreadable page can sink it, so a smaller dump is worth more than none.
    //
    // IndirectlyReferencedMemory is the one that earns its size here: it captures a
    // window around anything on a thread stack that looks like a pointer, which is
    // what lets you read the string an argument refers to rather than just its value.
    MINIDUMP_TYPE const attempts[] =
    {
        // IgnoreInaccessibleMemory is documented against MiniDumpWithFullMemory,
        // which this set does not request, so it is kept only as belt-and-braces on
        // implementations that honour it more broadly - the fallbacks below are what
        // actually cover an unreadable page here.
        MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory
                      | MiniDumpWithDataSegs
                      | MiniDumpWithProcessThreadData
                      | MiniDumpWithThreadInfo
                      | MiniDumpWithUnloadedModules
                      | MiniDumpWithFullMemoryInfo
                      | MiniDumpIgnoreInaccessibleMemory),
        MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
        MiniDumpNormal
    };

    bool written = false;

    for (int i = 0; i < ARRAYSIZE(attempts) && !written; ++i)
    {
        SetFilePointer(hDumpFile, 0, 0, FILE_BEGIN);
        SetEndOfFile(hDumpFile);

        written = MiniDumpWriteDump(GetCurrentProcess(),
                                    GetCurrentProcessId(),
                                    hDumpFile,
                                    attempts[i],
                                    &mei,
                                    0,
                                    0) != FALSE;
    }

    CloseHandle(hDumpFile);

    if (!written)
    {
        DeleteFile(m_szDumpFileName);
    }

    return written;
}

//===========================================================
// Runs WriteMiniDumpWorker on a thread of its own.
//===========================================================
DWORD WINAPI WheatyExceptionReport::MiniDumpThreadProc(LPVOID param)
{
    MiniDumpRequest* request = (MiniDumpRequest*)param;
    request->written = WriteMiniDumpWorker(request->pExceptionInfo, request->faultingThreadId);
    return 0;
}

//===========================================================
// Takes the dump from a dedicated thread.
//
// The reason for the extra thread is EXCEPTION_STACK_OVERFLOW: the filter runs on
// the exhausted stack with about a page to spare, and MiniDumpWriteDump needs far
// more, so the crash hardest to read from a text report is the one an in-filter dump
// is least likely to survive. Measured on one such crash: a 1.3 KB report and a
// complete dump. Not proven for every stack-overflow shape, only the tested one.
//
// The crashing thread blocks in WaitForSingleObject, so its stack stays intact and
// pExceptionInfo - which lives on it - remains valid for the helper to read.
//
// The timeout does NOT rescue the heap-lock deadlock case, and it would be wrong to
// claim it does. MiniDumpWriteDump suspends every thread in the process except its
// own caller, so the thread waiting here is suspended for the duration: the timeout
// expires in the kernel but the thread cannot run to act on it. That is true of any
// in-process watchdog on any thread - only a separate process can observe a hang
// while the dumper holds the process down. The timeout still bounds the cases where
// the helper fails or stalls before reaching MiniDumpWriteDump, which is worth
// having, but a dumper that deadlocks on a heap lock the faulting thread was holding
// will still freeze the process. Fixing that properly means an external watchdog.
//===========================================================
bool WheatyExceptionReport::WriteMiniDump(PEXCEPTION_POINTERS pExceptionInfo)
{
    // Static, not a local. On a timeout this function returns while the helper is
    // still running and still holding this pointer; a stack local would be reclaimed
    // underneath it, and the helper's later write to `written` would land in the
    // frame the report generator is by then using. The filter admits one caller
    // (see the guard in WheatyUnhandledExceptionFilter), so a single instance is
    // enough and no lifetime management is needed.
    static MiniDumpRequest request;

    request.pExceptionInfo = pExceptionInfo;
    request.faultingThreadId = GetCurrentThreadId();        // captured here, on the faulting thread
    request.written = false;

    // STACK_SIZE_PARAM_IS_A_RESERVATION: without it this number is the amount to
    // commit up front, not reserve, which makes creation likelier to fail on a
    // process that is already short of memory - and failing here drops us back to
    // dumping on the faulting stack, which is precisely what this thread exists to
    // avoid for stack-overflow crashes.
    HANDLE hThread = CreateThread(0, 256 * 1024, MiniDumpThreadProc, &request,
                                  STACK_SIZE_PARAM_IS_A_RESERVATION, 0);

    if (!hThread)
    {
        // No thread to be had - better a dump attempted on this stack than none.
        return WriteMiniDumpWorker(pExceptionInfo, request.faultingThreadId);
    }

    DWORD const waited = WaitForSingleObject(hThread, kMiniDumpTimeoutMs);

    // Not terminated on timeout: killing a thread inside MiniDumpWriteDump would
    // leave the threads it suspended suspended, and this process is exiting anyway.
    // But it is then still running, and probably still inside DbgHelp - which the
    // caller must know, because the text report uses DbgHelp too and the two cannot
    // safely overlap.
    CloseHandle(hThread);

    m_dumpHelperRunning = (waited != WAIT_OBJECT_0);

    return (waited == WAIT_OBJECT_0) && request.written;
}

//===========================================================
// Composes "<folder>\<name><suffix><ext>" without running past the buffer.
//
// Every part except the name is preserved. An installation directory long enough
// to threaten MAX_PATH is a valid one, and this runs on the crash path, so the
// question is only what to sacrifice: the timestamp, the pid and the extension
// are what make a crash pair identifiable, and the module name is the one part
// nothing depends on. So the name is what gets clipped.
//===========================================================
static void BuildCrashPath(TCHAR* out, size_t outSize, TCHAR const* folder,
                           TCHAR const* name, TCHAR const* suffix, TCHAR const* ext)
{
    size_t const fixed = _tcslen(folder) + 1 + _tcslen(suffix) + _tcslen(ext) + 1;
    int const nameRoom = (outSize > fixed) ? (int)(outSize - fixed) : 0;

    int written = _sntprintf(out, outSize, _T("%s\\%.*s%s%s"), folder, nameRoom, name, suffix, ext);

    // _sntprintf returns -1 on truncation as well as on error, and does not
    // terminate when it fills the buffer exactly.
    if (written < 0 || (size_t)written >= outSize)
    {
        // The folder alone leaves no room for the parts that had to survive. Drop it
        // and write beside the executable rather than emit a mangled path.
        _sntprintf(out, outSize, _T("crash%s%s"), suffix, ext);
    }

    out[outSize - 1] = _T('\0');
}

//===========================================================
// Entry point where control comes on an unhandled exception
//===========================================================
LONG WINAPI WheatyExceptionReport::WheatyUnhandledExceptionFilter(
    PEXCEPTION_POINTERS pExceptionInfo)
{
    // Everything below writes to statics - m_szLogFileName, m_hReportFile - with no
    // serialisation, so two threads faulting at once corrupt each other's filenames
    // and handles. A crash in the reporting path would also re-enter here. One entry
    // only; a second caller returns and lets the process die.
    static LONG s_inFilter = 0;

    if (InterlockedExchange(&s_inFilter, 1) != 0)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    TCHAR module_folder_name[MAX_PATH];
    DWORD const moduleLen = GetModuleFileName(0, module_folder_name, MAX_PATH);

    // 0 is failure; MAX_PATH means it filled the buffer, and older Windows does not
    // terminate in that case. Either way there is no usable path to split.
    if (moduleLen == 0 || moduleLen >= MAX_PATH)
    {
        InterlockedExchange(&s_inFilter, 0);
        return 0;
    }

    module_folder_name[MAX_PATH - 1] = _T('\0');
    TCHAR* pos = _tcsrchr(module_folder_name, '\\');
    if (!pos)
    {
        InterlockedExchange(&s_inFilter, 0);                // nothing was set up yet
        return 0;
    }
    pos[0] = '\0';
    ++pos;

    TCHAR crash_folder_path[MAX_PATH];
    int const folderLen = _sntprintf(crash_folder_path, ARRAYSIZE(crash_folder_path),
                                     _T("%s\\%s"), module_folder_name, CrashFolder);
    if (folderLen < 0 || folderLen >= ARRAYSIZE(crash_folder_path))
    {
        InterlockedExchange(&s_inFilter, 0);
        return 0;
    }
    crash_folder_path[ARRAYSIZE(crash_folder_path) - 1] = _T('\0');

    if (!CreateDirectory(crash_folder_path, NULL))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            InterlockedExchange(&s_inFilter, 0);
            return 0;
        }
    }

    SYSTEMTIME systime;
    GetLocalTime(&systime);

    // Year and process id are here because the previous form - day, month and time
    // of day only - repeats across years and across two runs that crash at the same
    // clock second. That was survivable while the report was the only artifact, since
    // it is opened OPEN_ALWAYS and appends. A dump cannot append: it is written
    // CREATE_ALWAYS, so a collision destroyed the older dump while its report was
    // still sitting in the same file as the newer one, and the pair no longer matched.
    // The process id also separates crashes from concurrent servers sharing a folder.
    TCHAR name_suffix[64];
    _sntprintf(name_suffix, ARRAYSIZE(name_suffix), _T("_[%u-%u-%u_%u-%u-%u_%u]"),
               systime.wDay, systime.wMonth, systime.wYear,
               systime.wHour, systime.wMinute, systime.wSecond, GetCurrentProcessId());
    name_suffix[ARRAYSIZE(name_suffix) - 1] = _T('\0');

    // Same suffix for both, so the .dmp and .txt for one crash always pair by name.
    BuildCrashPath(m_szLogFileName, ARRAYSIZE(m_szLogFileName), crash_folder_path, pos, name_suffix, _T(".txt"));
    BuildCrashPath(m_szDumpFileName, ARRAYSIZE(m_szDumpFileName), crash_folder_path, pos, name_suffix, _T(".dmp"));

    // Written before the report, deliberately. GenerateExceptionReport loads and
    // parses every module's symbols, walks all threads, and formats megabytes of
    // text - a great deal of work in a process that has already faulted, any of
    // which can fail and take the dump with it. The dump is also the artifact that
    // survives being mailed to someone else, so it is the one worth having first.
    m_dumpWritten = WriteMiniDump(pExceptionInfo);

    m_hReportFile = CreateFile(m_szLogFileName,
                               GENERIC_WRITE,
                               0,
                               0,
                               OPEN_ALWAYS,
                               FILE_FLAG_WRITE_THROUGH,
                               0);

    if (m_hReportFile)
    {
        SetFilePointer(m_hReportFile, 0, 0, FILE_END);

        if (m_dumpHelperRunning)
        {
            // The dump helper did not finish and was not killed, so it is very likely
            // still inside MiniDumpWriteDump - which is DbgHelp. DbgHelp is documented
            // single-threaded, and GenerateExceptionReport is nothing but DbgHelp:
            // SymInitialize, StackWalk64, SymEnumSymbols, SymCleanup. Running them
            // concurrently risks corrupting both, so the report is skipped rather than
            // racing the dump that is more useful anyway.
            _tprintf(_T("Revision: %s\r\n"), GitRevision::GetProjectRevision());
            _tprintf(_T("Minidump: timed out after %u ms; the writer was still running.\r\n"), kMiniDumpTimeoutMs);
            _tprintf(_T("Report skipped: generating it needs DbgHelp, which the dump writer still holds.\r\n"));
            _tprintf(_T("Partial dump, if any: %s\r\n"), m_szDumpFileName);
        }
        else
        {
            GenerateExceptionReport(pExceptionInfo);
        }

        CloseHandle(m_hReportFile);
        m_hReportFile = 0;
    }

    LONG const result = m_previousFilter
                        ? m_previousFilter(pExceptionInfo)
                        : EXCEPTION_EXECUTE_HANDLER/*EXCEPTION_CONTINUE_SEARCH*/;

    // A chained filter can return EXCEPTION_CONTINUE_EXECUTION, meaning it fixed the
    // fault and the process is to carry on. The guard must be released for that case
    // or this filter is dead for the life of the process: the next unhandled
    // exception would take the re-entry path, write nothing, skip the chained filter
    // that might have recovered again, and return EXCEPTION_EXECUTE_HANDLER - killing
    // a process that was still saveable. The other results end the process, and
    // releasing there would just let a second faulting thread into a reporting path
    // that is already unwinding.
    if (result == EXCEPTION_CONTINUE_EXECUTION)
    {
        InterlockedExchange(&s_inFilter, 0);
    }

    return result;
}

/**
 * Retrieves the processor name string from the Windows registry.
 */
BOOL WheatyExceptionReport::_GetProcessorName(TCHAR* sProcessorName, DWORD maxcount)
{
    if (!sProcessorName)
    {
        return FALSE;
    }

    HKEY hKey;
    LONG lRet;
    lRet = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
                          0, KEY_QUERY_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        return FALSE;
    }
    TCHAR szTmp[2048];
    DWORD cntBytes = sizeof(szTmp);
    lRet = ::RegQueryValueEx(hKey, _T("ProcessorNameString"), NULL, NULL,
                             (LPBYTE)szTmp, &cntBytes);
    if (lRet != ERROR_SUCCESS)
    {
        return FALSE;
    }
    ::RegCloseKey(hKey);
    sProcessorName[0] = '\0';
    // Skip spaces
    TCHAR* psz = szTmp;
    while (iswspace(*psz))
    {
        ++psz;
    }
    _tcsncpy(sProcessorName, psz, maxcount);
    return TRUE;
}

/**
 * Formats the current Windows version into a human-readable string.
 */
BOOL WheatyExceptionReport::_GetWindowsVersion(TCHAR* szVersion, DWORD cntMax)
{
    // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
    // If that fails, try using the OSVERSIONINFO structure.
    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    BOOL bOsVersionInfoEx;
    bOsVersionInfoEx = ::GetVersionEx((LPOSVERSIONINFO)(&osvi));
    if (!bOsVersionInfoEx)
    {
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!::GetVersionEx((OSVERSIONINFO*)&osvi))
        {
            return FALSE;
        }
    }
    *szVersion = _T('\0');
    TCHAR wszTmp[128];

    // print version
    switch (osvi.dwMajorVersion)
    {
        case 6:
            switch (osvi.dwMinorVersion)
            {
                default: // 2
                    if (osvi.wProductType == VER_NT_WORKSTATION)
                    {
                        _tcsncat(szVersion, _T("Windows 8 "), cntMax);
                    }
                    else
                    {
                        _tcsncat(szVersion, _T("Windows Server 2012 "), cntMax);
                    }
                    break;
                case 1:
                    if (osvi.wProductType == VER_NT_WORKSTATION)
                    {
                        _tcsncat(szVersion, _T("Windows 7 "), cntMax);
                    }
                    else
                    {
                        _tcsncat(szVersion, _T("Windows Server 2008 R2 "), cntMax);
                    }
                    break;
                case 0:
                    if (osvi.wProductType == VER_NT_WORKSTATION)
                    {
                        _tcsncat(szVersion, _T("Windows Vista "), cntMax);
                    }
                    else
                    {
                        _tcsncat(szVersion, _T("Windows Server 2008 "), cntMax);
                    }
                    break;
            }
            break;
        case 5:
            switch (osvi.dwMinorVersion)
            {
                default: // 2
                    _tcsncat(szVersion, _T("Windows Server 2003 "), cntMax);
                    break;
                case 1:
                    _tcsncat(szVersion, _T("Windows XP "), cntMax);
                    break;
                case 0:
                    _tcsncat(szVersion, _T("Windows 2000 "), cntMax);
                    break;
            }
            break;
        default:
            _tcsncat(szVersion, _T("Windows NT or lower "), cntMax);
            break;
    }

    // print service pack if one is installed
    if (_tcslen(osvi.szCSDVersion))
        _stprintf(wszTmp, _T("%s (Version %d.%d, Build %d)"),
                  osvi.szCSDVersion, osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber & 0xFFFF);
    else
        _stprintf(wszTmp, _T("(Version %d.%d, Build %d)"),
                  osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber & 0xFFFF);

    _tcsncat(szVersion, wszTmp, cntMax);

    return TRUE;
}

/**
 * Prints basic operating system, processor, and memory information.
 */
void WheatyExceptionReport::PrintSystemInfo()
{
    SYSTEM_INFO SystemInfo;
    ::GetSystemInfo(&SystemInfo);

    MEMORYSTATUS MemoryStatus;
    MemoryStatus.dwLength = sizeof(MEMORYSTATUS);
    ::GlobalMemoryStatus(&MemoryStatus);
    TCHAR sString[1024];
    _tprintf(_T("//=====================================================\r\n"));
    if (_GetProcessorName(sString, countof(sString)))
        _tprintf(_T("*** Hardware ***\r\nProcessor: %s\r\nNumber Of Processors: %d\r\nPhysical Memory: %d KB (Available: %d KB)\r\nCommit Charge Limit: %d KB\r\n"),
                 sString, SystemInfo.dwNumberOfProcessors, MemoryStatus.dwTotalPhys / 0x400, MemoryStatus.dwAvailPhys / 0x400, MemoryStatus.dwTotalPageFile / 0x400);
    else
        _tprintf(_T("*** Hardware ***\r\nProcessor: <unknown>\r\nNumber Of Processors: %d\r\nPhysical Memory: %d KB (Available: %d KB)\r\nCommit Charge Limit: %d KB\r\n"),
                 SystemInfo.dwNumberOfProcessors, MemoryStatus.dwTotalPhys / 0x400, MemoryStatus.dwAvailPhys / 0x400, MemoryStatus.dwTotalPageFile / 0x400);

    if (_GetWindowsVersion(sString, countof(sString)))
    {
        _tprintf(_T("\r\n*** Operation System ***\r\n%s\r\n"), sString);
    }
    else
    {
        _tprintf(_T("\r\n*** Operation System:\r\n<unknown>\r\n"));
    }
}

//===========================================================================
void WheatyExceptionReport::printTracesForAllThreads()
{
    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    THREADENTRY32 te32;

    DWORD dwOwnerPID = GetCurrentProcessId();
    m_hProcess = GetCurrentProcess();
    // Take a snapshot of all running threads
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
    {
        return;
    }

    // Fill in the size of the structure before using it.
    te32.dwSize = sizeof(THREADENTRY32);

    // Retrieve information about the first thread,
    // and exit if unsuccessful
    if (!Thread32First(hThreadSnap, &te32))
    {
        CloseHandle(hThreadSnap);      // Must clean up the
        //   snapshot object!
        return;
    }

    // Now walk the thread list of the system,
    // and display information about each thread
    // associated with the specified process
    do
    {
        if (te32.th32OwnerProcessID == dwOwnerPID)
        {
            CONTEXT context;
            context.ContextFlags = 0xffffffff;
            HANDLE threadHandle = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, false, te32.th32ThreadID);
            if (threadHandle && GetThreadContext(threadHandle, &context))
            {
                WriteStackDetails(&context, false, threadHandle);
            }
            CloseHandle(threadHandle);
        }
    }
    while (Thread32Next(hThreadSnap, &te32));

//  Don't forget to clean up the snapshot object.
    CloseHandle(hThreadSnap);
}

//===========================================================================
// Open the report file, and write the desired information to it.  Called by
// WheatyUnhandledExceptionFilter
//===========================================================================
void WheatyExceptionReport::GenerateExceptionReport(
    PEXCEPTION_POINTERS pExceptionInfo)
{
    SYSTEMTIME systime;
    GetLocalTime(&systime);

    // Start out with a banner
    _tprintf(_T("Revision: %s\r\n"), GitRevision::GetProjectRevision());
    _tprintf(_T("Date %u:%u:%u. Time %u:%u \r\n"), systime.wDay, systime.wMonth, systime.wYear, systime.wHour, systime.wMinute);

    // Named here so whoever opens the report knows the dump exists and where it is;
    // it carries the argument values this text cannot recover on optimised x64.
    if (m_szDumpFileName[0] && GetFileAttributes(m_szDumpFileName) != INVALID_FILE_ATTRIBUTES)
    {
        _tprintf(_T("Minidump: %s\r\n"), m_szDumpFileName);
    }
    else
    {
        _tprintf(_T("Minidump: not written\r\n"));
    }

    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    PrintSystemInfo();
    // First print information about the type of fault
    _tprintf(_T("\r\n//=====================================================\r\n"));
    _tprintf(_T("Exception code: %08X %s\r\n"),
             pExceptionRecord->ExceptionCode,
             GetExceptionString(pExceptionRecord->ExceptionCode));

    // Now print information about where the fault occured
    TCHAR szFaultingModule[MAX_PATH];
    DWORD section;
    DWORD_PTR offset;
    GetLogicalAddress(pExceptionRecord->ExceptionAddress,
                      szFaultingModule,
                      sizeof(szFaultingModule),
                      section, offset);

#ifdef _M_IX86
    _tprintf(_T("Fault address:  %08X %02X:%08X %s\r\n"),
             pExceptionRecord->ExceptionAddress,
             section, offset, szFaultingModule);
#endif

#ifdef _M_X64
    _tprintf(_T("Fault address:  %016I64X %02X:%016I64X %s\r\n"),
             pExceptionRecord->ExceptionAddress,
             section, offset, szFaultingModule);
#endif

    PCONTEXT pCtx = pExceptionInfo->ContextRecord;

    // Show the registers
#ifdef _M_IX86                                          // X86 Only!
    _tprintf(_T("\r\nRegisters:\r\n"));

    _tprintf(_T("EAX:%08X\r\nEBX:%08X\r\nECX:%08X\r\nEDX:%08X\r\nESI:%08X\r\nEDI:%08X\r\n")
            ,pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx,
             pCtx->Esi, pCtx->Edi);

    _tprintf(_T("CS:EIP:%04X:%08X\r\n"), pCtx->SegCs, pCtx->Eip);
    _tprintf(_T("SS:ESP:%04X:%08X  EBP:%08X\r\n"),
             pCtx->SegSs, pCtx->Esp, pCtx->Ebp);
    _tprintf(_T("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n"),
             pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs);
    _tprintf(_T("Flags:%08X\r\n"), pCtx->EFlags);
#endif

#ifdef _M_X64
    _tprintf(_T("\r\nRegisters:\r\n"));
    _tprintf(_T("RAX:%016I64X\r\nRBX:%016I64X\r\nRCX:%016I64X\r\nRDX:%016I64X\r\nRSI:%016I64X\r\nRDI:%016I64X\r\n")
            _T("R8: %016I64X\r\nR9: %016I64X\r\nR10:%016I64X\r\nR11:%016I64X\r\nR12:%016I64X\r\nR13:%016I64X\r\nR14:%016I64X\r\nR15:%016I64X\r\n")
            ,pCtx->Rax, pCtx->Rbx, pCtx->Rcx, pCtx->Rdx,
             pCtx->Rsi, pCtx->Rdi , pCtx->R9, pCtx->R10, pCtx->R11, pCtx->R12, pCtx->R13, pCtx->R14, pCtx->R15);
    _tprintf(_T("CS:RIP:%04X:%016I64X\r\n"), pCtx->SegCs, pCtx->Rip);
    _tprintf(_T("SS:RSP:%04X:%016X  RBP:%08X\r\n"),
             pCtx->SegSs, pCtx->Rsp, pCtx->Rbp);
    _tprintf(_T("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n"),
             pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs);
    _tprintf(_T("Flags:%08X\r\n"), pCtx->EFlags);
#endif

    SymSetOptions(SYMOPT_DEFERRED_LOADS);

    // Initialize DbgHelp
    if (!SymInitialize(GetCurrentProcess(), 0, TRUE))
    {
        _tprintf(_T("\n\rCRITICAL ERROR.\n\r Couldn't initialize the symbol handler for process.\n\rError [%s].\n\r\n\r"),
                 ErrorMessage(GetLastError()));
    }

    CONTEXT trashableContext = *pCtx;

    WriteStackDetails(&trashableContext, false, NULL);
    printTracesForAllThreads();

//    #ifdef _M_IX86                                        // X86 Only!

    _tprintf(_T("========================\r\n"));
    _tprintf(_T("Local Variables And Parameters\r\n"));

    trashableContext = *pCtx;
    WriteStackDetails(&trashableContext, true, NULL);

    _tprintf(_T("========================\r\n"));
    _tprintf(_T("Global Variables\r\n"));

    SymEnumSymbols(GetCurrentProcess(),
                   (DWORD64)GetModuleHandle(szFaultingModule),
                   0, EnumerateSymbolsCallback, 0);
    //  #endif                                              // X86 Only!

    SymCleanup(GetCurrentProcess());

    _tprintf(_T("\r\n"));
}

//======================================================================
// Given an exception code, returns a pointer to a static string with a
// description of the exception
//======================================================================
LPTSTR WheatyExceptionReport::GetExceptionString(DWORD dwCode)
{
#define EXCEPTION( x ) case EXCEPTION_##x: return _T(#x);

    switch (dwCode)
    {
            EXCEPTION(ACCESS_VIOLATION)
            EXCEPTION(DATATYPE_MISALIGNMENT)
            EXCEPTION(BREAKPOINT)
            EXCEPTION(SINGLE_STEP)
            EXCEPTION(ARRAY_BOUNDS_EXCEEDED)
            EXCEPTION(FLT_DENORMAL_OPERAND)
            EXCEPTION(FLT_DIVIDE_BY_ZERO)
            EXCEPTION(FLT_INEXACT_RESULT)
            EXCEPTION(FLT_INVALID_OPERATION)
            EXCEPTION(FLT_OVERFLOW)
            EXCEPTION(FLT_STACK_CHECK)
            EXCEPTION(FLT_UNDERFLOW)
            EXCEPTION(INT_DIVIDE_BY_ZERO)
            EXCEPTION(INT_OVERFLOW)
            EXCEPTION(PRIV_INSTRUCTION)
            EXCEPTION(IN_PAGE_ERROR)
            EXCEPTION(ILLEGAL_INSTRUCTION)
            EXCEPTION(NONCONTINUABLE_EXCEPTION)
            EXCEPTION(STACK_OVERFLOW)
            EXCEPTION(INVALID_DISPOSITION)
            EXCEPTION(GUARD_PAGE)
            EXCEPTION(INVALID_HANDLE)
    }

    // If not one of the "known" exceptions, try to get the string
    // from NTDLL.DLL's message table.

    static TCHAR szBuffer[512] = { 0 };

    FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                  GetModuleHandle(_T("NTDLL.DLL")),
                  dwCode, 0, szBuffer, sizeof(szBuffer), 0);

    return szBuffer;
}

//=============================================================================
// Given a linear address, locates the module, section, and offset containing
// that address.
//
// Note: the szModule paramater buffer is an output buffer of length specified
// by the len parameter (in characters!)
//=============================================================================
BOOL WheatyExceptionReport::GetLogicalAddress(
    PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD_PTR& offset)
{
    MEMORY_BASIC_INFORMATION mbi;

    if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
        return FALSE;
    }

    DWORD_PTR hMod = (DWORD_PTR)mbi.AllocationBase;

    if (!GetModuleFileName((HMODULE)hMod, szModule, len))
    {
        return FALSE;
    }

    // Point to the DOS header in memory
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;

    // From the DOS header, find the NT (PE) header
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + DWORD_PTR(pDosHdr->e_lfanew));

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHdr);

    DWORD_PTR rva = (DWORD_PTR)addr - hMod;                 // RVA is offset from module load address

    // Iterate through the section table, looking for the one that encompasses
    // the linear address.
    for (unsigned i = 0; i < pNtHdr->FileHeader.NumberOfSections; ++i, ++pSection)
    {
        DWORD_PTR sectionStart = pSection->VirtualAddress;
        DWORD_PTR sectionEnd = sectionStart + DWORD_PTR(pSection->SizeOfRawData > pSection->Misc.VirtualSize ? pSection->SizeOfRawData : pSection->Misc.VirtualSize);

        // Is the address in this section???
        if ((rva >= sectionStart) && (rva <= sectionEnd))
        {
            // Yes, address is in the section.  Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.
            section = i + 1;
            offset = rva - sectionStart;
            return TRUE;
        }
    }

    return FALSE;                                           // Should never get here!
}

// It contains SYMBOL_INFO structure plus additional
// space for the name of the symbol
struct CSymbolInfoPackage : public SYMBOL_INFO_PACKAGE
{
    CSymbolInfoPackage()
    {
        si.SizeOfStruct = sizeof(SYMBOL_INFO);
        si.MaxNameLen   = sizeof(name);
    }
};

//============================================================
// Walks the stack, and writes the results to the report file
//============================================================
void WheatyExceptionReport::WriteStackDetails(
    PCONTEXT pContext,
    bool bWriteVariables, HANDLE pThreadHandle)             // true if local/params should be output
{
    _tprintf(_T("\r\nCall stack:\r\n"));

    _tprintf(_T("Address   Frame     Function      SourceFile\r\n"));

    DWORD dwMachineType = 0;
    // Could use SymSetOptions here to add the SYMOPT_DEFERRED_LOADS flag

    STACKFRAME64 sf;
    memset(&sf, 0, sizeof(sf));

#ifdef _M_IX86
    // Initialize the STACKFRAME structure for the first call.  This is only
    // necessary for Intel CPUs, and isn't mentioned in the documentation.
    sf.AddrPC.Offset       = pContext->Eip;
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Esp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Ebp;
    sf.AddrFrame.Mode      = AddrModeFlat;

    dwMachineType = IMAGE_FILE_MACHINE_I386;
#endif

#ifdef _M_X64
    sf.AddrPC.Offset    = pContext->Rip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Rsp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Rbp;
    sf.AddrFrame.Mode      = AddrModeFlat;
    dwMachineType = IMAGE_FILE_MACHINE_AMD64;
#endif

    while (1)
    {
        // Get the next stack frame
        if (! StackWalk64(dwMachineType,
                          m_hProcess,
                          pThreadHandle != NULL ? pThreadHandle : GetCurrentThread(),
                          &sf,
                          pContext,
                          0,
                          SymFunctionTableAccess64,
                          SymGetModuleBase64,
                          0))
        {
            break;
        }
        if (0 == sf.AddrFrame.Offset)                       // Basic sanity check to make sure
        {
            break;                                           // the frame is OK.  Bail if not.
        }
#ifdef _M_IX86
        _tprintf(_T("%08X  %08X  "), sf.AddrPC.Offset, sf.AddrFrame.Offset);
#endif

#ifdef _M_X64
        _tprintf(_T("%016I64X  %016I64X  "), sf.AddrPC.Offset, sf.AddrFrame.Offset);
#endif

        DWORD64 symDisplacement = 0;                        // Displacement of the input address,
        // relative to the start of the symbol

        // Get the name of the function for this stack frame entry
        CSymbolInfoPackage sip;
        if (SymFromAddr(
                m_hProcess,                             // Process handle of the current process
                sf.AddrPC.Offset,                       // Symbol address
                &symDisplacement,                       // Address of the variable that will receive the displacement
                &sip.si))                               // Address of the SYMBOL_INFO structure (inside "sip" object)
        {
            _tprintf(_T("%hs+%I64X"), sip.si.Name, symDisplacement);
        }
        else                                                // No symbol found.  Print out the logical address instead.
        {
            TCHAR szModule[MAX_PATH] = _T("");
            DWORD section = 0;
            DWORD_PTR offset = 0;

            GetLogicalAddress((PVOID)sf.AddrPC.Offset,
                              szModule, sizeof(szModule), section, offset);
#ifdef _M_IX86
            _tprintf(_T("%04X:%08X %s"), section, offset, szModule);
#endif

#ifdef _M_X64
            _tprintf(_T("%04X:%016I64X %s"), section, offset, szModule);
#endif

        }

        // Get the source line for this stack frame entry
        IMAGEHLP_LINE64 lineInfo = { sizeof(IMAGEHLP_LINE) };
        DWORD dwLineDisplacement;
        if (SymGetLineFromAddr64(m_hProcess, sf.AddrPC.Offset,
                                 &dwLineDisplacement, &lineInfo))
        {
            _tprintf(_T("  %s line %u"), lineInfo.FileName, lineInfo.LineNumber);
        }

        _tprintf(_T("\r\n"));

        // Write out the variables, if desired
        if (bWriteVariables)
        {
            // Use SymSetContext to get just the locals/params for this frame
            IMAGEHLP_STACK_FRAME imagehlpStackFrame;
            imagehlpStackFrame.InstructionOffset = sf.AddrPC.Offset;
            SymSetContext(m_hProcess, &imagehlpStackFrame, 0);

            // Enumerate the locals/parameters
            SymEnumSymbols(m_hProcess, 0, 0, EnumerateSymbolsCallback, &sf);

            _tprintf(_T("\r\n"));
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// The function invoked by SymEnumSymbols
//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK
WheatyExceptionReport::EnumerateSymbolsCallback(
    PSYMBOL_INFO  pSymInfo,
    ULONG         SymbolSize,
    PVOID         UserContext)
{
    char szBuffer[kSymbolBufferSize];

    __try
    {
        if (FormatSymbolValue(pSymInfo, (STACKFRAME*)UserContext,
        szBuffer, sizeof(szBuffer)))
        {
            _tprintf(_T("\t%s\r\n"), szBuffer);
        }
    }
    __except(1)
    {
        _tprintf(_T("punting on symbol %s\r\n"), pSymInfo->Name);
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// Given a SYMBOL_INFO representing a particular variable, displays its
// contents.  If it's a user defined type, display the members and their
// values.
//////////////////////////////////////////////////////////////////////////////
bool WheatyExceptionReport::FormatSymbolValue(
    PSYMBOL_INFO pSym,
    STACKFRAME* sf,
    char* pszBuffer,
    unsigned cbBuffer)
{
    char* pszCurrBuffer = pszBuffer;
    char const* const pszEnd = pszBuffer + cbBuffer;

    // Indicate if the variable is a local or parameter
    if (pSym->Flags & IMAGEHLP_SYMBOL_INFO_PARAMETER)
    {
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "Parameter ");
    }
    else if (pSym->Flags & IMAGEHLP_SYMBOL_INFO_LOCAL)
    {
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "Local ");
    }

    // If it's a function, don't do anything.
    if (pSym->Tag == 5)                                     // SymTagFunction from CVCONST.H from the DIA SDK
    {
        return false;
    }

    DWORD_PTR pVariable = 0;                                // Will point to the variable's data in memory

    if (pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGRELATIVE)
    {
        // if ( pSym->Register == 8 )   // EBP is the value 8 (in DBGHELP 5.1)
        {
            //  This may change!!!
            pVariable = sf->AddrFrame.Offset;
            pVariable += (DWORD_PTR)pSym->Address;
        }
        // else
        //  return false;
    }
    else if (pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGISTER)
    {
        return false;                                       // Don't try to report register variable
    }
    else
    {
        pVariable = (DWORD_PTR)pSym->Address;               // It must be a global variable
    }

    // Determine if the variable is a user defined type (UDT).  IF so, bHandled
    // will return true.
    bool bHandled;
    pszCurrBuffer = DumpTypeIndex(pszCurrBuffer, pszEnd, pSym->ModBase, pSym->TypeIndex,
                                  0, pVariable, bHandled, pSym->Name);

    if (!bHandled)
    {
        // The symbol wasn't a UDT, so do basic, stupid formatting of the
        // variable.  Based on the size, we're assuming it's a char, WORD, or
        // DWORD.
        BasicType basicType = GetBasicType(pSym->TypeIndex, pSym->ModBase);
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, rgBaseType[basicType]);

        // Emit the variable name
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "\'%s\'", pSym->Name);

        pszCurrBuffer = FormatOutputValue(pszCurrBuffer, pszEnd, basicType, pSym->Size,
                                          (PVOID)pVariable);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// If it's a user defined type (UDT), recurse through its members until we're
// at fundamental types.  When he hit fundamental types, return
// bHandled = false, so that FormatSymbolValue() will format them.
//////////////////////////////////////////////////////////////////////////////
char* WheatyExceptionReport::DumpTypeIndex(
    char* pszCurrBuffer,
    char const* pszEnd,
    DWORD64 modBase,
    DWORD dwTypeIndex,
    unsigned nestingLevel,
    DWORD_PTR offset,
    bool& bHandled,
    char* Name)
{
    bHandled = false;

    // Get the name of the symbol.  This will either be a Type name (if a UDT),
    // or the structure member name.
    WCHAR* pwszTypeName;
    if (SymGetTypeInfo(m_hProcess, modBase, dwTypeIndex, TI_GET_SYMNAME,
                       &pwszTypeName))
    {
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " %ls", pwszTypeName);
        LocalFree(pwszTypeName);
    }

    // Determine how many children this type has.
    DWORD dwChildrenCount = 0;
    SymGetTypeInfo(m_hProcess, modBase, dwTypeIndex, TI_GET_CHILDRENCOUNT,
                   &dwChildrenCount);

    if (!dwChildrenCount)                                   // If no children, we're done
    {
        return pszCurrBuffer;
    }

    // Prepare to get an array of "TypeIds", representing each of the children.
    // SymGetTypeInfo(TI_FINDCHILDREN) expects more memory than just a
    // TI_FINDCHILDREN_PARAMS struct has.  Use derivation to accomplish this.
    struct FINDCHILDREN : TI_FINDCHILDREN_PARAMS
    {
        ULONG   MoreChildIds[1024];
        FINDCHILDREN()
        {
            Count = sizeof(MoreChildIds) / sizeof(MoreChildIds[0]);
        }
    } children;

    children.Count = dwChildrenCount;
    children.Start = 0;

    // Get the array of TypeIds, one for each child type
    if (!SymGetTypeInfo(m_hProcess, modBase, dwTypeIndex, TI_FINDCHILDREN,
                        &children))
    {
        return pszCurrBuffer;
    }

    // Append a line feed
    pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "\r\n");

    // Iterate through each of the children
    for (unsigned i = 0; i < dwChildrenCount; ++i)
    {
        // Add appropriate indentation level (since this routine is recursive)
        for (unsigned j = 0; j <= nestingLevel + 1; ++j)
        {
            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "\t");
        }

        // Recurse for each of the child types
        bool bHandled2;
        BasicType basicType = GetBasicType(children.ChildId[i], modBase);
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, rgBaseType[basicType]);

        pszCurrBuffer = DumpTypeIndex(pszCurrBuffer, pszEnd, modBase,
                                      children.ChildId[i], nestingLevel + 1,
                                      offset, bHandled2, ""/*Name */);

        // If the child wasn't a UDT, format it appropriately
        if (!bHandled2)
        {
            // Get the offset of the child member, relative to its parent
            DWORD dwMemberOffset;
            SymGetTypeInfo(m_hProcess, modBase, children.ChildId[i],
                           TI_GET_OFFSET, &dwMemberOffset);

            // Get the real "TypeId" of the child.  We need this for the
            // SymGetTypeInfo( TI_GET_TYPEID ) call below.
            DWORD typeId;
            SymGetTypeInfo(m_hProcess, modBase, children.ChildId[i],
                           TI_GET_TYPEID, &typeId);

            // Get the size of the child member
            ULONG64 length;
            SymGetTypeInfo(m_hProcess, modBase, typeId, TI_GET_LENGTH, &length);

            // Calculate the address of the member
            DWORD_PTR dwFinalOffset = offset + dwMemberOffset;

            //             BasicType basicType = GetBasicType(children.ChildId[i], modBase );
            //
            //          pszCurrBuffer += sprintf( pszCurrBuffer, rgBaseType[basicType]);
            //
            // Emit the variable name
            //          pszCurrBuffer += sprintf( pszCurrBuffer, "\'%s\'", Name );

            pszCurrBuffer = FormatOutputValue(pszCurrBuffer, pszEnd, basicType,
                                              length, (PVOID)dwFinalOffset);

            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, "\r\n");
        }
    }

    bHandled = true;
    return pszCurrBuffer;
}

char* WheatyExceptionReport::FormatOutputValue(char* pszCurrBuffer,
    char const* pszEnd,
        BasicType basicType,
        DWORD64 length,
        PVOID pAddress)
{
    // Format appropriately (assuming it's a 1, 2, or 4 bytes (!!!)
    if (length == 1)
    {
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %X", *(PBYTE)pAddress);
    }
    else if (length == 2)
    {
        pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %X", *(PWORD)pAddress);
    }
    else if (length == 4)
    {
        if (basicType == btFloat)
        {
            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %f", *(PFLOAT)pAddress);
        }
        else if (basicType == btChar)
        {
            if (!IsBadStringPtr(*(PSTR*)pAddress, 32))
            {
                // Was *(PDWORD)pAddress: the pointer is validated as a PSTR and then
                // only its low 32 bits were passed to %s, so on x64 the formatter was
                // handed a truncated address that the check above never vetted. The
                // compiler has been warning about this (C4313) the whole time.
                pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = \"%.31s\"",
                                         *(PSTR*)pAddress);
            }
            else
                pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %X",
                                         *(PDWORD)pAddress);
        }
        else
        {
            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %X", *(PDWORD)pAddress);
        }
    }
    else if (length == 8)
    {
        if (basicType == btFloat)
        {
            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %lf",
                                     *(double*)pAddress);
        }
        else
            pszCurrBuffer = AppendFormat(pszCurrBuffer, pszEnd, " = %I64X",
                                     *(DWORD64*)pAddress);
    }

    return pszCurrBuffer;
}

BasicType

/**
 * Resolves the basic type metadata for a symbol type index.
 */
WheatyExceptionReport::GetBasicType(DWORD typeIndex, DWORD64 modBase)
{
    BasicType basicType;
    if (SymGetTypeInfo(m_hProcess, modBase, typeIndex,
                       TI_GET_BASETYPE, &basicType))
    {
        return basicType;
    }

    // Get the real "TypeId" of the child.  We need this for the
    // SymGetTypeInfo( TI_GET_TYPEID ) call below.
    DWORD typeId;
    if (SymGetTypeInfo(m_hProcess, modBase, typeIndex, TI_GET_TYPEID, &typeId))
    {
        if (SymGetTypeInfo(m_hProcess, modBase, typeId, TI_GET_BASETYPE,
                           &basicType))
        {
            return basicType;
        }
    }

    return btNoType;
}

//============================================================================
// Bounded append into the symbol-formatting buffer. Returns the new write
// position, never past pszEnd, with the buffer always terminated.
//============================================================================
char* WheatyExceptionReport::AppendFormat(char* pszCurrBuffer, char const* pszEnd, char const* format, ...)
{
    if (!pszCurrBuffer || !pszEnd || pszCurrBuffer >= pszEnd)
    {
        return pszCurrBuffer;
    }

    size_t const remaining = size_t(pszEnd - pszCurrBuffer);

    va_list argptr;
    va_start(argptr, format);
    int const written = _vsnprintf(pszCurrBuffer, remaining, format, argptr);
    va_end(argptr);

    // _vsnprintf returns -1 when the output did not fit, and leaves the buffer
    // unterminated in that case.
    if (written < 0 || size_t(written) >= remaining)
    {
        pszCurrBuffer[remaining - 1] = '\0';
        return pszCurrBuffer + remaining - 1;
    }

    pszCurrBuffer[written] = '\0';
    return pszCurrBuffer + written;
}

//============================================================================
// Helper function that writes to the report file, and allows the user to use
// printf style formating
//============================================================================
int __cdecl WheatyExceptionReport::_tprintf(const TCHAR* format, ...)
{
    TCHAR szBuff[kReportLineSize];
    int retValue;
    DWORD cbWritten;
    va_list argptr;

    // vsprintf here was unbounded, and the WriteFile below then wrote the length it
    // returned - so a symbol dump longer than the buffer both smashed this frame and
    // flushed the overrun to disk. Bounded now, with the length clamped to what the
    // buffer actually holds.
    //
    // The buffer is sized from kSymbolBufferSize rather than picked independently:
    // callers hand this a whole formatted symbol plus their own wrapping, and a sink
    // smaller than that would trade the overflow for silent truncation - dropping the
    // tail of exactly the deep types this change exists to handle, line break
    // included, so the next symbol would run onto the same line.
    va_start(argptr, format);
    retValue = _vsnprintf(szBuff, ARRAYSIZE(szBuff) - 1, format, argptr);
    va_end(argptr);

    // _vsnprintf returns -1 when the output did not fit, and does not terminate.
    if (retValue < 0 || retValue > (int)(ARRAYSIZE(szBuff) - 1))
    {
        retValue = ARRAYSIZE(szBuff) - 1;
    }

    szBuff[retValue] = _T('\0');

    WriteFile(m_hReportFile, szBuff, retValue * sizeof(TCHAR), &cbWritten, 0);

    return retValue;
}

#include "StdAfx.h"
#include "../EterPack/EterPackManager.h"

#include "PythonLauncher.h"


// ============================================================
// METIN2_PYTHON315_PYCONFIG_V1
//
// Native x64 client uses an isolated CPython 3.15 runtime.
// Do not depend on machine-wide Python, registry Python,
// PYTHONHOME or PYTHONPATH.
//
// Runtime search priority:
//   1. <client>\python315
//   2. <client>\Python315
//   3. <client>\python
//   4. development fallback:
//      <client>\..\Project_ClientSrc\extern\Python315
// ============================================================

#if defined(_WIN64)
namespace
{

std::wstring Python315GetExecutablePath()
{
    std::vector<wchar_t> buffer(32768, L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

    if (
        length == 0 ||
        length >= buffer.size())
    {
        return {};
    }

    return std::wstring(
        buffer.data(),
        length);
}


std::wstring Python315GetExecutableDirectory()
{
    const std::wstring executable =
        Python315GetExecutablePath();

    if (executable.empty())
        return {};

    const std::wstring::size_type pos =
        executable.find_last_of(
            L"\\/");

    if (pos == std::wstring::npos)
        return L".";

    return executable.substr(
        0,
        pos);
}


std::wstring Python315NormalizePath(
    const std::wstring & path)
{
    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    const DWORD length =
        GetFullPathNameW(
            path.c_str(),
            static_cast<DWORD>(buffer.size()),
            buffer.data(),
            nullptr);

    if (
        length == 0 ||
        length >= buffer.size())
    {
        return path;
    }

    return std::wstring(
        buffer.data(),
        length);
}


bool Python315DirectoryExists(
    const std::wstring & path)
{
    const DWORD attributes =
        GetFileAttributesW(
            path.c_str());

    return
        attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}


bool Python315FileExists(
    const std::wstring & path)
{
    const DWORD attributes =
        GetFileAttributesW(
            path.c_str());

    return
        attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}


bool Python315HomeUsable(
    const std::wstring & home)
{
    if (!Python315DirectoryExists(home))
        return false;

    // encodings is required extremely early during
    // interpreter initialization.
    const std::wstring encodings =
        home +
        L"\\Lib\\encodings\\__init__.py";

    if (!Python315FileExists(encodings))
        return false;

    const std::wstring importlib =
        home +
        L"\\Lib\\importlib\\__init__.py";

    if (!Python315FileExists(importlib))
        return false;

    return true;
}


std::string Python315WideToUtf8(
    const std::wstring & value)
{
    if (value.empty())
        return {};

    const int required =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0)
        return {};

    std::string result(
        required,
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        required,
        nullptr,
        nullptr);

    return result;
}


std::wstring Python315ResolveHome()
{
    const std::wstring exeDir =
        Python315GetExecutableDirectory();

    if (exeDir.empty())
        return {};

    const std::wstring candidates[] =
    {
        exeDir + L"\\python315",
        exeDir + L"\\Python315",
        exeDir + L"\\python",

        // Current Project_Seferihisar development layout.
        exeDir +
            L"\\..\\Project_ClientSrc"
            L"\\extern\\Python315"
    };

    for (
        const std::wstring & candidate :
        candidates)
    {
        const std::wstring normalized =
            Python315NormalizePath(
                candidate);

        if (
            Python315HomeUsable(
                normalized))
        {
            return normalized;
        }
    }

    return {};
}


void Python315LogStatus(
    const char * stage,
    const PyStatus & status)
{
    TraceError(
        "PYTHON CONFIG FAILURE: %s | "
        "func=%s | error=%s",
        stage
            ? stage
            : "<unknown>",
        status.func
            ? status.func
            : "<none>",
        status.err_msg
            ? status.err_msg
            : "<none>");
}


bool Python315AppendSearchPath(
    PyConfig & config,
    const std::wstring & path)
{
    const PyStatus status =
        PyWideStringList_Append(
            &config.module_search_paths,
            path.c_str());

    if (PyStatus_Exception(status))
    {
        Python315LogStatus(
            "PyWideStringList_Append",
            status);

        return false;
    }

    TraceError(
        "PYTHON PATH: %s",
        Python315WideToUtf8(
            path).c_str());

    return true;
}


bool Python315Initialize()
{
    if (Py_IsInitialized())
    {
        TraceError(
            "PYTHON CONFIG: interpreter "
            "already initialized");

        return true;
    }

    const std::wstring executable =
        Python315GetExecutablePath();

    const std::wstring executableDir =
        Python315GetExecutableDirectory();

    const std::wstring pythonHome =
        Python315ResolveHome();

    if (executable.empty())
    {
        TraceError(
            "PYTHON CONFIG FAILURE: "
            "could not resolve executable path");

        return false;
    }

    if (pythonHome.empty())
    {
        TraceError(
            "PYTHON CONFIG FAILURE: "
            "Python 3.15 home not found");

        TraceError(
            "PYTHON CONFIG SEARCH ROOT: %s",
            Python315WideToUtf8(
                executableDir).c_str());

        return false;
    }

    TraceError(
        "PYTHON CONFIG: executable=%s",
        Python315WideToUtf8(
            executable).c_str());

    TraceError(
        "PYTHON CONFIG: home=%s",
        Python315WideToUtf8(
            pythonHome).c_str());

    PyConfig config;

    PyConfig_InitIsolatedConfig(
        &config);

    // Fully embedded and deterministic runtime.
    config.use_environment = 0;
    config.parse_argv = 0;
    config.user_site_directory = 0;
    config.site_import = 0;
    config.write_bytecode = 0;
    config.install_signal_handlers = 0;
    config.pathconfig_warnings = 1;

    PyStatus status =
        PyConfig_SetString(
            &config,
            &config.program_name,
            executable.c_str());

    if (PyStatus_Exception(status))
    {
        Python315LogStatus(
            "program_name",
            status);

        PyConfig_Clear(
            &config);

        return false;
    }

    status =
        PyConfig_SetString(
            &config,
            &config.home,
            pythonHome.c_str());

    if (PyStatus_Exception(status))
    {
        Python315LogStatus(
            "home",
            status);

        PyConfig_Clear(
            &config);

        return false;
    }

    // Do not let CPython guess sys.path from
    // Windows registry or system installations.
    config.module_search_paths_set = 1;

    const std::wstring pythonZip =
        pythonHome +
        L"\\python315.zip";

    if (
        Python315FileExists(
            pythonZip))
    {
        if (
            !Python315AppendSearchPath(
                config,
                pythonZip))
        {
            PyConfig_Clear(
                &config);

            return false;
        }
    }

    const std::wstring dlls =
        pythonHome +
        L"\\DLLs";

    if (
        Python315DirectoryExists(
            dlls))
    {
        if (
            !Python315AppendSearchPath(
                config,
                dlls))
        {
            PyConfig_Clear(
                &config);

            return false;
        }
    }

    const std::wstring standardLibrary =
        pythonHome +
        L"\\Lib";

    if (
        !Python315AppendSearchPath(
            config,
            standardLibrary))
    {
        PyConfig_Clear(
            &config);

        return false;
    }

    // Allows modules living directly in Python home.
    if (
        !Python315AppendSearchPath(
            config,
            pythonHome))
    {
        PyConfig_Clear(
            &config);

        return false;
    }

    const std::wstring sitePackages =
        pythonHome +
        L"\\Lib\\site-packages";

    if (
        Python315DirectoryExists(
            sitePackages))
    {
        if (
            !Python315AppendSearchPath(
                config,
                sitePackages))
        {
            PyConfig_Clear(
                &config);

            return false;
        }
    }

    // Keep sys.argv deterministic even though
    // Python itself will not parse Metin2 arguments.
    status =
        PyWideStringList_Append(
            &config.argv,
            executable.c_str());

    if (PyStatus_Exception(status))
    {
        Python315LogStatus(
            "argv",
            status);

        PyConfig_Clear(
            &config);

        return false;
    }

    TraceError(
        "PYTHON CONFIG: "
        "Py_InitializeFromConfig begin");

    status =
        Py_InitializeFromConfig(
            &config);

    if (PyStatus_Exception(status))
    {
        Python315LogStatus(
            "Py_InitializeFromConfig",
            status);

        PyConfig_Clear(
            &config);

        return false;
    }

    PyConfig_Clear(
        &config);

    TraceError(
        "PYTHON CONFIG: "
        "Py_InitializeFromConfig OK; "
        "initialized=%d",
        Py_IsInitialized()
            ? 1
            : 0);

    return
        Py_IsInitialized() != 0;
}

}
#endif


CPythonLauncher::CPythonLauncher()
{
#if defined(_WIN64)
    TraceError(
        "PYTHON INIT 01: "
        "PyConfig initialization begin");

    if (!Python315Initialize())
    {
        TraceError(
            "PYTHON INIT FAILURE: "
            "CPython 3.15 initialization failed");

        return;
    }

    TraceError(
        "PYTHON INIT 02: "
        "PyConfig initialization OK");
#else
    Py_Initialize();
#endif
}

CPythonLauncher::~CPythonLauncher()
{
    Clear();
}

void CPythonLauncher::Clear()
{
    if (Py_IsInitialized())
        Py_Finalize();
}

std::string g_stTraceBuffer[512];
int g_nCurTraceN = 0;

namespace
{
std::string PythonObjectToUtf8(PyObject* obj)
{
    if (!obj)
        return {};

#if defined(_WIN64)
    PyObject* text = PyObject_Str(obj);
    if (!text)
    {
        PyErr_Clear();
        return {};
    }

    const char* value = PyUnicode_AsUTF8(text);
    std::string result = value ? value : "";
    Py_DECREF(text);
    return result;
#else
    if (PyString_Check(obj))
    {
        const char* value = PyString_AsString(obj);
        return value ? value : "";
    }

    PyObject* text = PyObject_Str(obj);
    if (!text)
        return {};

    const char* value = PyString_AsString(text);
    std::string result = value ? value : "";
    Py_DECREF(text);
    return result;
#endif
}

#if defined(_WIN64)
void GetFrameDescription(PyFrameObject* frame, std::string& fileName, std::string& functionName, int& line)
{
    fileName.clear();
    functionName.clear();
    line = PyFrame_GetLineNumber(frame);

    PyCodeObject* code = PyFrame_GetCode(frame); // new reference
    if (!code)
    {
        PyErr_Clear();
        return;
    }

    PyObject* codeObject = reinterpret_cast<PyObject*>(code);
    PyObject* fileObject = PyObject_GetAttrString(codeObject, "co_filename");
    PyObject* nameObject = PyObject_GetAttrString(codeObject, "co_name");

    fileName = PythonObjectToUtf8(fileObject);
    functionName = PythonObjectToUtf8(nameObject);

    Py_XDECREF(fileObject);
    Py_XDECREF(nameObject);
    Py_DECREF(code);
}
#endif
}

void Traceback()
{
#if defined(_WIN64)

    // METIN2_PYTHON315_NATIVE_TRACEBACK_V2
    //
    // Do NOT import Python's traceback module here.
    // system.py installs its own import machinery, so an
    // error reporter must not depend on another Python import.
    //
    // PyErr_GetRaisedException returns the actual exception
    // object including its attached __traceback__.
    //
    // PyErr_DisplayException prints the standard full
    // traceback directly to sys.stderr.

    PyObject* exc =
        PyErr_GetRaisedException();

    if (!exc)
    {
        TraceError(
            "PYTHON TRACEBACK: "
            "<no active exception>");

        return;
    }

    TraceError(
        "============================================================");

    TraceError(
        "PYTHON TRACEBACK BEGIN");

    TraceError(
        "Exception object: %s",
        PythonObjectToUtf8(
            exc).c_str());

    // CPython 3.12+ native traceback renderer.
    // No importlib / traceback module / custom importer involved.
    PyErr_DisplayException(
        exc);

    // sys.stderr may have been replaced by system.py.
    // If DisplayException itself reported an error, do not
    // let that secondary logger error poison the client.
    if (PyErr_Occurred())
    {
        PyObject* displayError =
            PyErr_GetRaisedException();

        TraceError(
            "PYTHON TRACEBACK DISPLAY ERROR: %s",
            PythonObjectToUtf8(
                displayError).c_str());

        Py_XDECREF(
            displayError);
    }

    TraceError(
        "PYTHON TRACEBACK END");

    TraceError(
        "============================================================");

    Py_DECREF(
        exc);

    MessageBoxA(
        nullptr,
        "Python runtime error.\n"
        "Full traceback written to\n"
        "syserr/client_syserr.txt",
        "PYTHON ERROR",
        MB_OK | MB_ICONERROR);

#else

    // Legacy Win32/Python2 path.
    std::string str;

    for (
        int i = 0;
        i < g_nCurTraceN;
        ++i)
    {
        str.append(
            g_stTraceBuffer[i]);

        str.append(
            "\n");
    }

    PyObject* exc = nullptr;
    PyObject* value = nullptr;
    PyObject* tb = nullptr;

    PyErr_Fetch(
        &exc,
        &value,
        &tb);

    const std::string error =
        PythonObjectToUtf8(
            value
                ? value
                : exc);

    if (!error.empty())
    {
        str.append(
            "Error: ");

        str.append(
            error);
    }

    Py_XDECREF(exc);
    Py_XDECREF(value);
    Py_XDECREF(tb);

    TraceError(
        "PYTHON TRACEBACK:\n%s",
        str.c_str());

#endif
}

int TraceFunc(PyObject* obj, PyFrameObject* frame, int what, PyObject* arg)
{
    (void)obj;
    (void)arg;

    char szTraceBuffer[256]{};

#if defined(_WIN64)
    std::string fileName;
    std::string functionName;
    int line = 0;

    switch (what)
    {
    case PyTrace_CALL:
        if (g_nCurTraceN >= 512)
            return 0;

        GetFrameDescription(frame, fileName, functionName, line);
        _snprintf_s(
            szTraceBuffer,
            sizeof(szTraceBuffer),
            _TRUNCATE,
            "Call: File \"%s\", line %d, in %s",
            fileName.c_str(),
            line,
            functionName.c_str());
        g_stTraceBuffer[g_nCurTraceN++] = szTraceBuffer;
        break;

    case PyTrace_RETURN:
        if (g_nCurTraceN > 0)
            --g_nCurTraceN;
        break;

    case PyTrace_EXCEPTION:
        if (g_nCurTraceN >= 512)
            return 0;

        GetFrameDescription(frame, fileName, functionName, line);
        _snprintf_s(
            szTraceBuffer,
            sizeof(szTraceBuffer),
            _TRUNCATE,
            "Exception: File \"%s\", line %d, in %s",
            fileName.c_str(),
            line,
            functionName.c_str());
        g_stTraceBuffer[g_nCurTraceN++] = szTraceBuffer;
        break;
    }
#else
    const char* funcname;

    switch (what)
    {
    case PyTrace_CALL:
        if (g_nCurTraceN >= 512)
            return 0;

        if (Py_OptimizeFlag)
            frame->f_lineno = PyCode_Addr2Line(frame->f_code, frame->f_lasti);

        funcname = PyString_AsString(frame->f_code->co_name);

        snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Call: File \"%s\", line %d, in %s",
                  PyString_AsString(frame->f_code->co_filename), frame->f_lineno, funcname);

        g_stTraceBuffer[g_nCurTraceN++] = szTraceBuffer;
        break;

    case PyTrace_RETURN:
        if (g_nCurTraceN > 0)
            --g_nCurTraceN;
        break;

    case PyTrace_EXCEPTION:
        if (g_nCurTraceN >= 512)
            return 0;

        snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Exception: File \"%s\", line %d, in %s",
                  PyString_AS_STRING(frame->f_code->co_filename), frame->f_lineno, PyString_AS_STRING(frame->f_code->co_name));

        g_stTraceBuffer[g_nCurTraceN++] = szTraceBuffer;
        break;
    }
#endif

    return 0;
}

void CPythonLauncher::SetTraceFunc(int (*pFunc)(PyObject* obj, PyFrameObject* f, int what, PyObject* arg))
{
    PyEval_SetTrace(pFunc, nullptr);
}

bool CPythonLauncher::Create(const char* c_szProgramName)
{

#if defined(_WIN64)
    // METIN2_PYTHON315_CREATE_GUARD_V1
    if (!Py_IsInitialized())
    {
        TraceError(
            "PYTHON STARTUP: Create called "
            "without initialized interpreter");

        return false;
    }
#endif

    NANOBEGIN

#if !defined(_WIN64)
    Py_SetProgramName((char*)c_szProgramName);
#else
    (void)c_szProgramName;
#endif

#ifdef _DEBUG
    PyEval_SetTrace(TraceFunc, nullptr);
#endif

    m_poModule = PyImport_AddModule("__main__");
    if (!m_poModule)
    {
        TraceError("PYTHON STARTUP: PyImport_AddModule(__main__) failed");

        if (PyErr_Occurred())
            Traceback();

        return false;
    }

    m_poDic = PyModule_GetDict(m_poModule);

    if (!m_poDic)
    {
        TraceError("PYTHON STARTUP: PyModule_GetDict(__main__) failed");

        if (PyErr_Occurred())
            Traceback();

        return false;
    }

#if defined(_WIN64)
    PyObject* builtins = PyImport_ImportModule("builtins");
#else
    PyObject* builtins = PyImport_ImportModule("__builtin__");
#endif

    if (!builtins)
    {
        TraceError("PYTHON STARTUP: builtins import failed");

        if (PyErr_Occurred())
            Traceback();

        return false;
    }

    PyModule_AddIntConstant(builtins, "TRUE", 1);
    PyModule_AddIntConstant(builtins, "FALSE", 0);
    PyDict_SetItemString(m_poDic, "__builtins__", builtins);
    Py_DECREF(builtins);

    if (!RunLine("import __main__"))
    {
        TraceError("PYTHON STARTUP: import __main__ failed");
        return false;
    }

    if (!RunLine("import sys"))
    {
        TraceError("PYTHON STARTUP: import sys failed");
        return false;
    }

    TraceError("PYTHON STARTUP: CPythonLauncher::Create OK");

    NANOEND
    return true;
}

bool CPythonLauncher::RunCompiledFile(const char* c_szFileName)
{
    NANOBEGIN

    msl::file_ptr fPtr(c_szFileName, "rb");
    if (!fPtr)
        return false;

    long magic = _PyMarshal_ReadLongFromFile(fPtr.get());
    if (magic != PyImport_GetMagicNumber())
    {
        PyErr_SetString(PyExc_RuntimeError, "Bad magic number in .pyc file");
        return false;
    }

#if defined(_WIN64)
    // CPython 3.7+ uses a fixed 16-byte pyc header:
    // magic (4), flags (4), then timestamp/size or hash payload (8).
    if (fseek(fPtr.get(), 12, SEEK_CUR) != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid .pyc header");
        return false;
    }
#else
    _PyMarshal_ReadLongFromFile(fPtr.get());
#endif

    PyObject* codeObject = _PyMarshal_ReadLastObjectFromFile(fPtr.get());
    fPtr.close();

    if (!codeObject || !PyCode_Check(codeObject))
    {
        Py_XDECREF(codeObject);
        PyErr_SetString(PyExc_RuntimeError, "Bad code object in .pyc file");
        return false;
    }

#if defined(_WIN64)
    PyObject* result = PyEval_EvalCode(codeObject, m_poDic, m_poDic);
#else
    PyObject* result = PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(codeObject), m_poDic, m_poDic);
#endif

    Py_DECREF(codeObject);

    if (!result)
    {
        Traceback();
        return false;
    }

    Py_DECREF(result);
    if (Py_FlushLine())
        PyErr_Clear();

    NANOEND
    return true;
}

bool CPythonLauncher::RunMemoryTextFile(
    const char* c_szFileName,
    uint32_t uFileSize,
    const VOID* c_pvFileData)
{
    NANOBEGIN

    const auto* c_pcFileData =
        static_cast<const CHAR*>(c_pvFileData);

#if defined(_WIN64)

    // METIN2_PY315_DIRECT_SOURCE_EXEC_V1
    //
    // Python 2 legacy implementation wrapped the entire source:
    //
    //     legacy nested exec-compile source wrapper
    //
    // That is unsafe on Python 3 because escape sequences inside
    // SOURCE are parsed by the outer string first.
    //
    // Example:
    //     b"\xff\xfe\x00\x00"
    //
    // causes the outer parser to create literal NUL characters.
    //
    // Compile the original source buffer directly instead.

    if (!c_pcFileData)
    {
        PyErr_SetString(
            PyExc_RuntimeError,
            "RunMemoryTextFile received null source buffer");

        Traceback();

        return false;
    }

    std::string source(
        c_pcFileData,
        c_pcFileData + uFileSize);

    // Python source files must never contain literal NUL bytes.
    if (source.find('\0') != std::string::npos)
    {
        PyErr_Format(
            PyExc_ValueError,
            "Python source contains literal NUL byte: %s",
            c_szFileName
                ? c_szFileName
                : "<unknown>");

        Traceback();

        return false;
    }

    // Be defensive against UTF-8 BOM files.
    if (
        source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF)
    {
        source.erase(
            0,
            3);
    }

    TraceError(
        "PYTHON SOURCE: compile begin | file=%s | size=%u",
        c_szFileName
            ? c_szFileName
            : "<unknown>",
        static_cast<unsigned int>(source.size()));

    PyObject* codeObject =
        Py_CompileString(
            source.c_str(),
            c_szFileName
                ? c_szFileName
                : "<memory>",
            Py_file_input);

    if (!codeObject)
    {
        TraceError(
            "PYTHON SOURCE: compile FAILED | file=%s",
            c_szFileName
                ? c_szFileName
                : "<unknown>");

        Traceback();

        return false;
    }

    TraceError(
        "PYTHON SOURCE: compile OK | file=%s",
        c_szFileName
            ? c_szFileName
            : "<unknown>");

    PyObject* result =
        PyEval_EvalCode(
            codeObject,
            m_poDic,
            m_poDic);

    Py_DECREF(
        codeObject);

    if (!result)
    {
        TraceError(
            "PYTHON SOURCE: eval FAILED | file=%s",
            c_szFileName
                ? c_szFileName
                : "<unknown>");

        Traceback();

        return false;
    }

    Py_DECREF(
        result);

    if (Py_FlushLine())
        PyErr_Clear();

    TraceError(
        "PYTHON SOURCE: eval OK | file=%s",
        c_szFileName
            ? c_szFileName
            : "<unknown>");

#else

    // Legacy Python 2 / Win32 behavior remains untouched.

    std::string stConvFileData;

    stConvFileData.reserve(
        uFileSize);

    stConvFileData +=
        "exec(compile('''";

    for (
        uint32_t i = 0;
        i < uFileSize;
        ++i)
    {
        if (c_pcFileData[i] != 13)
            stConvFileData +=
                c_pcFileData[i];
    }

    stConvFileData +=
        "''', '";

    stConvFileData +=
        c_szFileName;

    stConvFileData +=
        "', 'exec'))";

    const CHAR* c_pcConvFileData =
        stConvFileData.c_str();

    NANOEND

    return RunLine(
        c_pcConvFileData);

#endif

    NANOEND

    return true;
}

bool CPythonLauncher::RunFile(const char* c_szFileName)
{
    std::vector<char> acBufData;
    uint32_t dwBufSize = 0;

    {
        CMappedFile file;
        const VOID* pvData;
        CEterPackManager::Instance().Get(file, c_szFileName, &pvData);

        dwBufSize = file.Size();
        if (dwBufSize == 0)
            return false;

        acBufData.resize(dwBufSize);
        memcpy(acBufData.data(), pvData, acBufData.size());
    }

    return RunMemoryTextFile(c_szFileName, dwBufSize, acBufData.data());
}

bool CPythonLauncher::RunLine(const char* c_szSrc)
{
    PyObject* v = PyRun_String(c_szSrc, Py_file_input, m_poDic, m_poDic);

    if (!v)
    {
        Traceback();
        return false;
    }

    Py_DECREF(v);
    return true;
}

const char* CPythonLauncher::GetError()
{
    static thread_local std::string error;
    error.clear();

    PyObject* exc = nullptr;
    PyObject* value = nullptr;
    PyObject* tb = nullptr;
    PyErr_Fetch(&exc, &value, &tb);

    error = PythonObjectToUtf8(value ? value : exc);

    Py_XDECREF(exc);
    Py_XDECREF(value);
    Py_XDECREF(tb);

    return error.c_str();
}

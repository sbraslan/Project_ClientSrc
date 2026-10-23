#include "StdAfx.h"
#include "../EterPack/EterPackManager.h"

#include "PythonLauncher.h"

CPythonLauncher::CPythonLauncher()
{
    Py_Initialize();
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
    std::string str;

    for (int i = 0; i < g_nCurTraceN; ++i)
    {
        str.append(g_stTraceBuffer[i]);
        str.append("\n");
    }

    PyObject* exc = nullptr;
    PyObject* value = nullptr;
    PyObject* tb = nullptr;
    PyErr_Fetch(&exc, &value, &tb);

    const std::string error = PythonObjectToUtf8(value ? value : exc);
    if (!error.empty())
    {
        str.append("Error: ");
        str.append(error);
        Tracef("%s\n", error.c_str());
    }

    Py_XDECREF(exc);
    Py_XDECREF(value);
    Py_XDECREF(tb);

    LogBoxf("Traceback:\n\n%s\n", str.c_str());
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

        _snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Call: File \"%s\", line %d, in %s",
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

        _snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Exception: File \"%s\", line %d, in %s",
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
        return false;

    m_poDic = PyModule_GetDict(m_poModule);

#if defined(_WIN64)
    PyObject* builtins = PyImport_ImportModule("builtins");
#else
    PyObject* builtins = PyImport_ImportModule("__builtin__");
#endif
    if (!builtins)
        return false;

    PyModule_AddIntConstant(builtins, "TRUE", 1);
    PyModule_AddIntConstant(builtins, "FALSE", 0);
    PyDict_SetItemString(m_poDic, "__builtins__", builtins);
    Py_DECREF(builtins);

    if (!RunLine("import __main__"))
        return false;

    if (!RunLine("import sys"))
        return false;

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

bool CPythonLauncher::RunMemoryTextFile(const char* c_szFileName, uint32_t uFileSize, const VOID* c_pvFileData)
{
    NANOBEGIN
    const auto* c_pcFileData = (const CHAR*)c_pvFileData;

    std::string stConvFileData;
    stConvFileData.reserve(uFileSize);
    stConvFileData += "exec(compile('''";

    for (uint32_t i = 0; i < uFileSize; ++i)
    {
        if (c_pcFileData[i] != 13)
            stConvFileData += c_pcFileData[i];
    }

    stConvFileData += "''', '";
    stConvFileData += c_szFileName;
    stConvFileData += "', 'exec'))";

    const CHAR* c_pcConvFileData = stConvFileData.c_str();
    NANOBEGIN
    const bool result = RunLine(c_pcConvFileData);
    NANOEND
    return result;
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

#pragma once

#if defined(_WIN64)

// METIN2_PY315_LEGACY_CODEPAGE_BRIDGE_V1
//
// Legacy Metin2 text rendering works with the currently selected
// Windows code page (1254 TR, 1251 RU, 1250 PL/CZ, etc.).
//
// CPython 3 uses Unicode internally. These helpers preserve the old
// Python 2 byte-string behaviour at the C++ boundary.

uint32_t GetDefaultCodePage();

inline bool PyUnicode_ToLegacyCodePageCompat(
    PyObject* obj,
    char** buffer,
    Py_ssize_t* length)
{
    if (!obj || !buffer || !length)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (!PyUnicode_Check(obj))
    {
        PyErr_SetString(
            PyExc_TypeError,
            "expected Python unicode string");

        return false;
    }

    Py_ssize_t wideLength = 0;

    wchar_t* wideText =
        PyUnicode_AsWideCharString(
            obj,
            &wideLength);

    if (!wideText)
        return false;

    UINT codePage =
        static_cast<UINT>(
            GetDefaultCodePage());

    if (codePage == 0)
        codePage = CP_UTF8;

    static thread_local std::string storage[32];
    static thread_local unsigned int storageIndex = 0;

    std::string& result =
        storage[
            storageIndex++ %
            (sizeof(storage) / sizeof(storage[0]))];

    if (wideLength == 0)
    {
        result.assign(1, '\0');

        PyMem_Free(wideText);

        *buffer = result.data();
        *length = 0;

        return true;
    }

    const int required =
        WideCharToMultiByte(
            codePage,
            0,
            wideText,
            static_cast<int>(wideLength),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0)
    {
        PyMem_Free(wideText);

        PyErr_SetString(
            PyExc_UnicodeError,
            "failed to encode Python Unicode using Metin2 code page");

        return false;
    }

    result.resize(
        static_cast<size_t>(required) + 1);

    const int converted =
        WideCharToMultiByte(
            codePage,
            0,
            wideText,
            static_cast<int>(wideLength),
            result.data(),
            required,
            nullptr,
            nullptr);

    PyMem_Free(wideText);

    if (converted != required)
    {
        PyErr_SetString(
            PyExc_UnicodeError,
            "incomplete Metin2 code page conversion");

        return false;
    }

    result[static_cast<size_t>(required)] = '\0';

    *buffer = result.data();
    *length =
        static_cast<Py_ssize_t>(required);

    return true;
}

inline PyObject* PyUnicode_FromLegacyCodePageCompat(
    const char* data,
    Py_ssize_t length)
{
    if (!data)
    {
        PyErr_BadInternalCall();
        return nullptr;
    }

    if (length < 0)
    {
        length =
            static_cast<Py_ssize_t>(
                std::strlen(data));
    }

    if (length == 0)
        return PyUnicode_FromStringAndSize("", 0);

    UINT codePage =
        static_cast<UINT>(
            GetDefaultCodePage());

    if (codePage == 0)
        codePage = CP_UTF8;

    const int wideLength =
        MultiByteToWideChar(
            codePage,
            0,
            data,
            static_cast<int>(length),
            nullptr,
            0);

    if (wideLength <= 0)
    {
        PyErr_SetString(
            PyExc_UnicodeError,
            "failed to decode Metin2 locale bytes");

        return nullptr;
    }

    std::wstring wideText(
        static_cast<size_t>(wideLength),
        L'\0');

    const int converted =
        MultiByteToWideChar(
            codePage,
            0,
            data,
            static_cast<int>(length),
            wideText.data(),
            wideLength);

    if (converted != wideLength)
    {
        PyErr_SetString(
            PyExc_UnicodeError,
            "incomplete Metin2 locale byte conversion");

        return nullptr;
    }

    return PyUnicode_FromWideChar(
        wideText.data(),
        static_cast<Py_ssize_t>(
            wideText.size()));
}

inline bool PyString_CheckCompat(PyObject* obj)
{
    return obj &&
        (PyUnicode_Check(obj) || PyBytes_Check(obj));
}

inline char* PyString_AsStringCompat(PyObject* obj)
{
    if (!obj)
        return nullptr;

    if (PyUnicode_Check(obj))
    {
        char* value = nullptr;
        Py_ssize_t length = 0;

        if (!PyUnicode_ToLegacyCodePageCompat(
                obj,
                &value,
                &length))
        {
            return nullptr;
        }

        return value;
    }

    if (PyBytes_Check(obj))
        return PyBytes_AsString(obj);

    PyErr_SetString(
        PyExc_TypeError,
        "expected str or bytes");

    return nullptr;
}

inline int PyObject_AsCharBufferCompat(
    PyObject* obj,
    const char** buffer,
    Py_ssize_t* length)
{
    if (!obj || !buffer || !length)
    {
        PyErr_BadInternalCall();
        return -1;
    }

    if (PyUnicode_Check(obj))
    {
        char* value = nullptr;

        if (!PyUnicode_ToLegacyCodePageCompat(
                obj,
                &value,
                length))
        {
            return -1;
        }

        *buffer = value;
        return 0;
    }

    if (PyBytes_Check(obj))
    {
        char* value = nullptr;

        if (PyBytes_AsStringAndSize(
                obj,
                &value,
                length) < 0)
        {
            return -1;
        }

        *buffer = value;
        return 0;
    }

    PyObject* text =
        PyObject_Str(obj);

    if (!text)
        return -1;

    char* value = nullptr;

    if (!PyUnicode_ToLegacyCodePageCompat(
            text,
            &value,
            length))
    {
        Py_DECREF(text);
        return -1;
    }

    Py_DECREF(text);

    *buffer = value;
    return 0;
}

inline PyObject* Py_InitModuleCompat(
    const char* name,
    PyMethodDef* methods)
{
    PyObject* module =
        PyModule_New(name);

    if (!module)
        return nullptr;

    if (PyModule_AddFunctions(module, methods) < 0)
    {
        Py_DECREF(module);
        return nullptr;
    }

    PyObject* modules =
        PyImport_GetModuleDict();

    if (!modules ||
        PyDict_SetItemString(
            modules,
            name,
            module) < 0)
    {
        Py_DECREF(module);
        return nullptr;
    }

    Py_DECREF(module);

    return PyDict_GetItemString(
        modules,
        name);
}

inline int Py_FlushLineCompat()
{
    return PyRun_SimpleString(
        "import sys\n"
        "getattr(sys.stdout, 'flush', lambda: None)()\n"
        "getattr(sys.stderr, 'flush', lambda: None)()\n");
}

#define Py_InitModule(name, methods) \
    Py_InitModuleCompat((name), (methods))

#define Py_InitModule3(name, methods, doc) \
    Py_InitModuleCompat((name), (methods))

#define PyInt_Check PyLong_Check
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AsLong PyLong_AsLong
#define PyInt_AS_LONG PyLong_AsLong
#define _PyInt_AsInt(obj) \
    static_cast<int>(PyLong_AsLong((obj)))

#define PyString_Check PyString_CheckCompat
#define PyString_AsString PyString_AsStringCompat
#define PyString_AS_STRING PyString_AsStringCompat

// Keep global creation semantics unchanged for now.
// Locale pack decoding is handled specifically by PythonPackModule.
#define PyString_FromString PyUnicode_FromString
#define PyString_FromStringAndSize PyUnicode_FromStringAndSize
#define PyString_InternFromString PyUnicode_InternFromString

#define PyObject_AsCharBuffer PyObject_AsCharBufferCompat
#define Py_FlushLine Py_FlushLineCompat

#endif
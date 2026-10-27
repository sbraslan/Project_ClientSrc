#pragma once

#include <cstring>
#include <string>

inline PyObject* Py_BuildLocaleStringUI(const char* text)
{
#if defined(_WIN64)
    if (!text)
        return PyUnicode_FromString("");

    const Py_ssize_t textLen =
        static_cast<Py_ssize_t>(std::strlen(text));

    if (textLen == 0)
        return PyUnicode_FromString("");

    const UINT codePage =
        static_cast<UINT>(LocaleService_GetCodePage());

    if (codePage == CP_UTF8)
        return PyUnicode_DecodeUTF8(
            text,
            textLen,
            "strict"
        );

    const int wideLen = MultiByteToWideChar(
        codePage,
        0,
        text,
        static_cast<int>(textLen),
        nullptr,
        0
    );

    if (wideLen <= 0)
    {
        PyErr_SetFromWindowsErr(
            static_cast<int>(GetLastError())
        );
        return nullptr;
    }

    std::wstring wide(
        static_cast<size_t>(wideLen),
        L'\0'
    );

    const int converted = MultiByteToWideChar(
        codePage,
        0,
        text,
        static_cast<int>(textLen),
        &wide[0],
        wideLen
    );

    if (converted != wideLen)
    {
        PyErr_SetFromWindowsErr(
            static_cast<int>(GetLastError())
        );
        return nullptr;
    }

    return PyUnicode_FromWideChar(
        &wide[0],
        static_cast<Py_ssize_t>(wide.size())
    );
#else
    return Py_BuildValue(
        "s",
        text ? text : ""
    );
#endif
}

inline PyObject* Py_BuildLocaleStringAutoUI(const char* text)
{
#if defined(_WIN64)
    if (!text)
        return PyUnicode_FromString("");

    const Py_ssize_t textLen =
        static_cast<Py_ssize_t>(std::strlen(text));

    if (textLen == 0)
        return PyUnicode_FromString("");

    PyObject* utf8 = PyUnicode_DecodeUTF8(
        text,
        textLen,
        "strict"
    );

    if (utf8)
        return utf8;

    PyErr_Clear();

    return Py_BuildLocaleStringUI(text);
#else
    return Py_BuildValue(
        "s",
        text ? text : ""
    );
#endif
}

inline PyObject* Py_BuildLocaleArgs_IS(
    int value,
    const char* text)
{
    PyObject* tuple = PyTuple_New(2);
    if (!tuple)
        return nullptr;

    PyObject* pyValue = PyLong_FromLong(value);
    PyObject* pyText = Py_BuildLocaleStringUI(text);

    if (!pyValue || !pyText)
    {
        Py_XDECREF(pyValue);
        Py_XDECREF(pyText);
        Py_DECREF(tuple);
        return nullptr;
    }

    PyTuple_SET_ITEM(tuple, 0, pyValue);
    PyTuple_SET_ITEM(tuple, 1, pyText);

    return tuple;
}

inline PyObject* Py_BuildLocaleArgs_SI(
    const char* text,
    int value)
{
    PyObject* tuple = PyTuple_New(2);
    if (!tuple)
        return nullptr;

    PyObject* pyText = Py_BuildLocaleStringUI(text);
    PyObject* pyValue = PyLong_FromLong(value);

    if (!pyText || !pyValue)
    {
        Py_XDECREF(pyText);
        Py_XDECREF(pyValue);
        Py_DECREF(tuple);
        return nullptr;
    }

    PyTuple_SET_ITEM(tuple, 0, pyText);
    PyTuple_SET_ITEM(tuple, 1, pyValue);

    return tuple;
}

inline PyObject* Py_BuildLocaleArgs_ISSS(
    int value,
    const char* text1,
    const char* text2,
    const char* text3)
{
    PyObject* tuple = PyTuple_New(4);
    if (!tuple)
        return nullptr;

    PyObject* p0 = PyLong_FromLong(value);
    PyObject* p1 = Py_BuildLocaleStringUI(text1);
    PyObject* p2 = Py_BuildLocaleStringUI(text2);
    PyObject* p3 = Py_BuildLocaleStringUI(text3);

    if (!p0 || !p1 || !p2 || !p3)
    {
        Py_XDECREF(p0);
        Py_XDECREF(p1);
        Py_XDECREF(p2);
        Py_XDECREF(p3);
        Py_DECREF(tuple);
        return nullptr;
    }

    PyTuple_SET_ITEM(tuple, 0, p0);
    PyTuple_SET_ITEM(tuple, 1, p1);
    PyTuple_SET_ITEM(tuple, 2, p2);
    PyTuple_SET_ITEM(tuple, 3, p3);

    return tuple;
}
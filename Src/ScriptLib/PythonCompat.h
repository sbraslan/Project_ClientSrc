#pragma once

#if defined(_WIN64)

inline bool PyString_CheckCompat(PyObject* obj)
{
    return obj && (PyUnicode_Check(obj) || PyBytes_Check(obj));
}

inline char* PyString_AsStringCompat(PyObject* obj)
{
    if (!obj)
        return nullptr;

    if (PyUnicode_Check(obj))
        return const_cast<char*>(PyUnicode_AsUTF8(obj));

    if (PyBytes_Check(obj))
        return PyBytes_AsString(obj);

    PyErr_SetString(PyExc_TypeError, "expected str or bytes");
    return nullptr;
}

inline int PyObject_AsCharBufferCompat(PyObject* obj, const char** buffer, Py_ssize_t* length)
{
    if (!obj || !buffer || !length)
    {
        PyErr_BadInternalCall();
        return -1;
    }

    if (PyUnicode_Check(obj))
    {
        const char* value = PyUnicode_AsUTF8AndSize(obj, length);
        if (!value)
            return -1;
        *buffer = value;
        return 0;
    }

    if (PyBytes_Check(obj))
    {
        char* value = nullptr;
        if (PyBytes_AsStringAndSize(obj, &value, length) < 0)
            return -1;
        *buffer = value;
        return 0;
    }

    PyObject* text = PyObject_Str(obj);
    if (!text)
        return -1;

    const char* value = PyUnicode_AsUTF8AndSize(text, length);
    if (!value)
    {
        Py_DECREF(text);
        return -1;
    }

    // This compatibility helper is only used immediately by legacy call sites.
    // Copy into thread-local storage so the returned pointer survives DECREF.
    static thread_local std::string storage;
    storage.assign(value, static_cast<size_t>(*length));
    Py_DECREF(text);

    *buffer = storage.c_str();
    *length = static_cast<Py_ssize_t>(storage.size());
    return 0;
}

inline PyObject* Py_InitModuleCompat(const char* name, PyMethodDef* methods)
{
    PyObject* module = PyModule_New(name);
    if (!module)
        return nullptr;

    if (PyModule_AddFunctions(module, methods) < 0)
    {
        Py_DECREF(module);
        return nullptr;
    }

    PyObject* modules = PyImport_GetModuleDict(); // borrowed
    if (!modules || PyDict_SetItemString(modules, name, module) < 0)
    {
        Py_DECREF(module);
        return nullptr;
    }

    Py_DECREF(module);
    return PyDict_GetItemString(modules, name); // borrowed, matches legacy usage
}

inline int Py_FlushLineCompat()
{
    return PyRun_SimpleString(
        "import sys\n"
        "getattr(sys.stdout, 'flush', lambda: None)()\n"
        "getattr(sys.stderr, 'flush', lambda: None)()\n");
}

#define Py_InitModule(name, methods) Py_InitModuleCompat((name), (methods))
#define Py_InitModule3(name, methods, doc) Py_InitModuleCompat((name), (methods))

#define PyInt_Check PyLong_Check
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AsLong PyLong_AsLong
#define PyInt_AS_LONG PyLong_AsLong
#define _PyInt_AsInt(obj) static_cast<int>(PyLong_AsLong((obj)))

#define PyString_Check PyString_CheckCompat
#define PyString_AsString PyString_AsStringCompat
#define PyString_AS_STRING PyString_AsStringCompat
#define PyString_FromString PyUnicode_FromString
#define PyString_FromStringAndSize PyUnicode_FromStringAndSize
#define PyString_InternFromString PyUnicode_InternFromString

#define PyObject_AsCharBuffer PyObject_AsCharBufferCompat
#define Py_FlushLine Py_FlushLineCompat

#endif

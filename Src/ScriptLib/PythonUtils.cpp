#include "StdAfx.h"
#include "PythonUtils.h"
#include <limits>
#define PyLong_AsLong PyLong_AsLongLong
#define PyLong_AsUnsignedLong PyLong_AsUnsignedLongLong
IPythonExceptionSender * g_pkExceptionSender = nullptr;

bool __PyCallClassMemberFunc_ByCString(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, PyObject ** ppoRet);
bool __PyCallClassMemberFunc_ByPyString(PyObject * poClass, PyObject * poFuncName, PyObject * poArgs, PyObject ** ppoRet);
bool __PyCallClassMemberFunc(PyObject * poClass, PyObject * poFunc, PyObject * poArgs, PyObject ** ppoRet);

PyObject * Py_BadArgument()
{
	PyErr_BadArgument();
	return nullptr;
}

PyObject * Py_BuildException(const char * c_pszErr, ...)
{
	if (!c_pszErr)
		PyErr_Clear();
	else
	{
		char szErrBuf[512 + 1];
		va_list args;
		va_start(args, c_pszErr);
		vsnprintf(szErrBuf, sizeof(szErrBuf), c_pszErr, args);
		va_end(args);

		PyErr_SetString(PyExc_RuntimeError, szErrBuf);
	}

	return Py_BuildNone();
	//return nullptr;
}

PyObject * Py_BuildNone()
{
	Py_INCREF(Py_None);
	return Py_None;
}

void Py_ReleaseNone()
{
	Py_DECREF(Py_None);
}

bool PyTuple_GetObject(PyObject * poArgs, int pos, PyObject ** ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = poItem;
	return true;
}

// METIN2_PY315_NUMERIC_BRIDGE_V1
//
// Python 2 UI code frequently relied on integer division.
// Under Python 3 the same expressions produce float values.
//
// C++ APIs that semantically require integer coordinates,
// dimensions, slots or indices therefore accept Python numeric
// values and explicitly convert them to an integer here.
//
// Most importantly: never return success while a Python
// exception is still set.

static bool PyObject_ToSignedIntegerCompat(
    PyObject* poItem,
    long long* pValue)
{
    if (!poItem || !pValue)
    {
        PyErr_BadInternalCall();
        return false;
    }

    PyObject* poLong =
        PyNumber_Long(poItem);

    if (!poLong)
        return false;

    const long long value =
        PyLong_AsLongLong(poLong);

    Py_DECREF(poLong);

    if (PyErr_Occurred())
        return false;

    *pValue = value;

    return true;
}

static bool PyObject_ToUnsignedIntegerCompat(
    PyObject* poItem,
    unsigned long long* pValue)
{
    if (!poItem || !pValue)
    {
        PyErr_BadInternalCall();
        return false;
    }

    PyObject* poLong =
        PyNumber_Long(poItem);

    if (!poLong)
        return false;

    const unsigned long long value =
        PyLong_AsUnsignedLongLong(poLong);

    Py_DECREF(poLong);

    if (PyErr_Occurred())
        return false;

    *pValue = value;

    return true;
}

bool PyTuple_GetLong(
    PyObject* poArgs,
    int pos,
    long* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    long long value = 0;

    if (!PyObject_ToSignedIntegerCompat(
            poItem,
            &value))
        return false;

    if (
        value <
            static_cast<long long>(
                std::numeric_limits<long>::min())
        ||
        value >
            static_cast<long long>(
                std::numeric_limits<long>::max())
    )
    {
        PyErr_SetString(
            PyExc_OverflowError,
            "integer does not fit C long");

        return false;
    }

    *ret = static_cast<long>(value);

    return true;
}

bool PyTuple_GetLongLong(
    PyObject* poArgs,
    int pos,
    long long* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    return PyObject_ToSignedIntegerCompat(
        poItem,
        ret);
}

bool PyTuple_GetDouble(PyObject * poArgs, int pos, double * ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = PyFloat_AsDouble(poItem);
	return true;
}

bool PyTuple_GetFloat(PyObject * poArgs, int pos, float * ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = static_cast<float>(PyFloat_AsDouble(poItem));
	return true;
}

bool PyTuple_GetByte(PyObject * poArgs, int pos, unsigned char * ret)
{
	int val;
	bool result = PyTuple_GetInteger(poArgs, pos, &val);
	*ret = static_cast<unsigned char>(val);
	return result;
}

bool PyTuple_GetInteger(PyObject * poArgs, int pos, unsigned char * ret)
{
	int val;
	bool result = PyTuple_GetInteger(poArgs, pos, &val);
	*ret = static_cast<unsigned char>(val);
	return result;
}

bool PyTuple_GetInteger(PyObject * poArgs, int pos, uint16_t * ret)
{
	int val;
	bool result = PyTuple_GetInteger(poArgs, pos, &val);
	*ret = static_cast<uint16_t>(val);
	return result;
}

bool PyTuple_GetInteger(
    PyObject* poArgs,
    int pos,
    int* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    long long value = 0;

    if (!PyObject_ToSignedIntegerCompat(
            poItem,
            &value))
        return false;

    if (
        value <
            static_cast<long long>(
                std::numeric_limits<int>::min())
        ||
        value >
            static_cast<long long>(
                std::numeric_limits<int>::max())
    )
    {
        PyErr_SetString(
            PyExc_OverflowError,
            "integer does not fit C int");

        return false;
    }

    *ret = static_cast<int>(value);

    return true;
}

bool PyTuple_GetUnsignedLong(
    PyObject* poArgs,
    int pos,
    unsigned long* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    unsigned long long value = 0;

    if (!PyObject_ToUnsignedIntegerCompat(
            poItem,
            &value))
        return false;

    if (
        value >
        static_cast<unsigned long long>(
            std::numeric_limits<unsigned long>::max())
    )
    {
        PyErr_SetString(
            PyExc_OverflowError,
            "integer does not fit C unsigned long");

        return false;
    }

    *ret =
        static_cast<unsigned long>(value);

    return true;
}

bool PyTuple_GetUnsignedInteger(
    PyObject* poArgs,
    int pos,
    unsigned int* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    unsigned long long value = 0;

    if (!PyObject_ToUnsignedIntegerCompat(
            poItem,
            &value))
        return false;

    if (
        value >
        static_cast<unsigned long long>(
            std::numeric_limits<unsigned int>::max())
    )
    {
        PyErr_SetString(
            PyExc_OverflowError,
            "integer does not fit C unsigned int");

        return false;
    }

    *ret =
        static_cast<unsigned int>(value);

    return true;
}

bool PyTuple_GetString(PyObject * poArgs, int pos, char ** ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	if (!PyString_Check(poItem))
		return false;

	*ret = PyString_AsString(poItem);
	return true;
}

bool PyTuple_GetBoolean(
    PyObject* poArgs,
    int pos,
    bool* ret)
{
    if (!ret)
    {
        PyErr_BadInternalCall();
        return false;
    }

    if (pos < 0 || pos >= PyTuple_Size(poArgs))
        return false;

    PyObject* poItem =
        PyTuple_GetItem(poArgs, pos);

    if (!poItem)
        return false;

    const int truth =
        PyObject_IsTrue(poItem);

    if (truth < 0)
        return false;

    *ret = (truth != 0);

    return true;
}

bool PyCallClassMemberFunc(PyObject * poClass, PyObject * poFunc, PyObject * poArgs)
{
	PyObject * poRet{};

	// NOTE : nullptr 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc(poClass, poFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs)
{
	PyObject * poRet{};

	// NOTE : nullptr 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc_ByPyString(PyObject * poClass, PyObject * poFuncName, PyObject * poArgs)
{
	PyObject * poRet{};

	// NOTE : nullptr 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByPyString(poClass, poFuncName, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, bool * pisRet)
{
	PyObject * poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	if (PyNumber_Check(poRet))
		*pisRet = (PyLong_AsLong(poRet) != 0);
	else
		*pisRet = true;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, long * plRetValue)
{
	PyObject * poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	if (PyNumber_Check(poRet))
	{
		*plRetValue = PyLong_AsLong(poRet);
		Py_DECREF(poRet);
		return true;
	}

	Py_DECREF(poRet);
	return false;
}

/*
 *	이 함수를 직접 호출하지 않도록 한다.
 *	부득이 하게 직접 호출할 경우에는 반드시 false 가 리턴 됐을 때
 *	Py_DECREF(poArgs); 를 해준다.
 */
bool __PyCallClassMemberFunc_ByCString(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, PyObject ** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttrString(poClass, c_szFunc); // New Reference

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs); // New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc_ByPyString(PyObject * poClass, PyObject * poFuncName, PyObject * poArgs, PyObject ** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttr(poClass, poFuncName); // New Reference

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs); // New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc(PyObject * poClass, PyObject * poFunc, PyObject * poArgs, PyObject ** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs); // New Reference

	if (!poRet)
	{
		PyErr_Print();
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

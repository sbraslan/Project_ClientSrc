#pragma once

#include <cstdint>

#define SET_EXCEPTION(x) PyErr_SetString(PyExc_RuntimeError, #x)

bool PyTuple_GetString(PyObject * poArgs, int pos, char ** ret);
bool PyTuple_GetInteger(PyObject * poArgs, int pos, unsigned char * ret);
bool PyTuple_GetInteger(PyObject * poArgs, int pos, int * ret);
bool PyTuple_GetInteger(PyObject * poArgs, int pos, uint16_t * ret);
bool PyTuple_GetByte(PyObject * poArgs, int pos, unsigned char * ret);
bool PyTuple_GetUnsignedInteger(PyObject * poArgs, int pos, unsigned int * ret);
bool PyTuple_GetLong(PyObject * poArgs, int pos, long * ret);
bool PyTuple_GetLongLong(PyObject* poArgs, int pos, long long* ret);
bool PyTuple_GetUnsignedLong(PyObject * poArgs, int pos, unsigned long * ret);
bool PyTuple_GetFloat(PyObject * poArgs, int pos, float * ret);
bool PyTuple_GetDouble(PyObject * poArgs, int pos, double * ret);
bool PyTuple_GetObject(PyObject * poArgs, int pos, PyObject ** ret);
bool PyTuple_GetBoolean(PyObject * poArgs, int pos, bool * ret);

template <typename T>
bool PyTuple_GetPointer(PyObject * poArgs, int pos, T ** ret)
{
	if (!ret)
		return false;

#if defined(_WIN64)
	if (pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);
	if (!poItem)
		return false;

	void * pValue = PyLong_AsVoidPtr(poItem);
	if (PyErr_Occurred())
		return false;

	if (!pValue)
		return false;

	*ret = static_cast<T *>(pValue);
	return true;
#else
	int handle = 0;
	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	*ret = reinterpret_cast<T *>(
		static_cast<std::intptr_t>(handle)
	);
	return true;
#endif
}

template <typename T>
PyObject * Py_BuildPointer(T * ptr)
{
#if defined(_WIN64)
	return PyLong_FromVoidPtr(static_cast<void *>(ptr));
#else
	return Py_BuildValue(
		"i",
		static_cast<int>(
			reinterpret_cast<std::intptr_t>(ptr)
		)
	);
#endif
}

bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs);
bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, bool * pisRet);
bool PyCallClassMemberFunc(PyObject * poClass, const char * c_szFunc, PyObject * poArgs, long * plRetValue);

bool PyCallClassMemberFunc_ByPyString(PyObject * poClass, PyObject * poFuncName, PyObject * poArgs);
bool PyCallClassMemberFunc(PyObject * poClass, PyObject * poFunc, PyObject * poArgs);

PyObject * Py_BuildException(const char * c_pszErr = nullptr, ...);
PyObject * Py_BadArgument();
PyObject * Py_BuildNone();
PyObject * Py_BuildEmptyTuple();

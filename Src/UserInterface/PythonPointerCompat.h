#pragma once

#include <Python.h>

template <typename T>
bool PyTuple_GetPointer(
	PyObject* args,
	int index,
	T** out)
{
	if (!out)
		return false;

	PyObject* obj = PyTuple_GetItem(args, index);

	if (!obj)
		return false;

	void* ptr = PyLong_AsVoidPtr(obj);

	if (PyErr_Occurred())
		return false;

	*out = static_cast<T*>(ptr);
	return true;
}

template <typename T>
PyObject* Py_BuildPointer(T* ptr)
{
	return PyLong_FromVoidPtr(static_cast<void*>(ptr));
}
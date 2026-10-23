#pragma once

#include "../eterLib/StdAfx.h"
#include "../eterGrnLib/StdAfx.h"

// Python runtime headers.
// Win32 keeps the legacy Python 2 ABI for the frozen baseline.
// Native x64 is migrated to the current CPython 3.15 SDK.
extern "C"
{
#ifdef AT
#	undef AT // @warme667
#endif

#if defined(_WIN64)
#	ifdef _DEBUG
#		undef _DEBUG
#		include "../../Extern/Python315/include/Python.h"
#		define _DEBUG
#	else
#		include "../../Extern/Python315/include/Python.h"
#	endif
#	include "../../Extern/Python315/include/marshal.h"
#	include "../../Extern/Python315/include/frameobject.h"
#else
#	ifdef _DEBUG
#		undef _DEBUG
#		include <Python2/Python.h>
#		define _DEBUG
#	else
#		include <Python2/Python.h>
#	endif
#	include <Python2/node.h>
#	include <Python2/grammar.h>
#	include <Python2/token.h>
#	include <Python2/parsetok.h>
#	include <Python2/errcode.h>
#	include <Python2/compile.h>
#	include <Python2/symtable.h>
#	include <Python2/eval.h>
#	include <Python2/marshal.h>
#	include <Python2/frameobject.h>
#	include <Python2/longintrepr.h>
#endif

#ifdef AT
#	undef AT // @warme667
#endif
}

#include "PythonCompat.h"
#include "PythonUtils.h"
#include "PythonLauncher.h"
#include "PythonMarshal.h"
#include "Resource.h"

void initdbg();

// PYTHON_EXCEPTION_SENDER
class IPythonExceptionSender
{
public:
	void Clear() { m_strExceptionString = ""; }

	void RegisterExceptionString(const char * c_szString) { m_strExceptionString += c_szString; }

	virtual void Send() = 0;

protected:
	std::string m_strExceptionString;
};

extern IPythonExceptionSender * g_pkExceptionSender;

void SetExceptionSender(IPythonExceptionSender * pkExceptionSender);
// END_OF_PYTHON_EXCEPTION_SENDER

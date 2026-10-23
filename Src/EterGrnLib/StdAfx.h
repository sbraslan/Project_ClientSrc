#pragma once

#include "../EterBase/LegacyCRTCompat.h"

#pragma warning(disable : 4786) // character 255 ³Ñ¾î°¡´Â°Å ²ô±â

#if defined(_WIN64) && __has_include("../../Extern/Granny/2.11.10.0/x64/include/granny2/granny.h")
#	define METIN2_GRANNY_X64_SDK 1
#	include "../../Extern/Granny/2.11.10.0/x64/include/granny2/granny.h"
#else
// Migration fallback: keep compiling against the proven legacy headers until
// the licensed x64 Granny SDK is installed. Final Win64 linking still requires
// a real x64 Granny runtime/import library.
#	include <granny2/granny.h>
#endif

#include "../EterBase/StdAfx.h"
#include "../EterBase/Utils.h"
#include "../EterBase/Debug.h"
#include "../EterBase/Stl.h"

#include "Util.h"

// Armadillo nanomite protection
#ifndef NANOBEGIN
#	ifdef __BORLANDC__
#		define NANOBEGIN __emit__(0xEB, 0x03, 0xD6, 0xD7, 0x01)
#		define NANOEND __emit__(0xEB, 0x03, 0xD6, 0xD7, 0x00)
#	else
#		define NANOBEGIN __asm _emit 0xEB __asm _emit 0x03 __asm _emit 0xD6 __asm _emit 0xD7 __asm _emit 0x01
#		define NANOEND __asm _emit 0xEB __asm _emit 0x03 __asm _emit 0xD6 __asm _emit 0xD7 __asm _emit 0x00
#	endif
#endif

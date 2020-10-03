/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

//NOTE: I know, we use unsafe string copy functions
#define _CRT_SECURE_NO_WARNINGS

// NOTE: This trick is needed to bind assembly manifest to the current version of the VC CRT
// See also: http://msdn.microsoft.com/ru-ru/library/cc664727.aspx
#define _BIND_TO_CURRENT_CRT_VERSION 1

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER              // Allow use of features specific to Windows XP or later.
#define WINVER 0x0501       // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT        // Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501 // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS      // Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE           // Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600    // Change this to the appropriate value to target other versions of IE.
#endif

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Define C99 stdint.h types for VS2008
#ifdef _MSC_VER
   typedef signed   __int8   int8_t;
   typedef unsigned __int8   uint8_t;
   typedef signed   __int16  int16_t;
   typedef unsigned __int16  uint16_t;
   typedef signed   __int32  int32_t;
   typedef unsigned __int32  uint32_t;
   typedef unsigned __int64  uint64_t;

#  define false   0
#  define true    1
#  define bool int
#else
#include <stdint.h>
#include <stdbool.h>
#endif

// TODO: reference additional headers your program requires here

#include "Common.h"

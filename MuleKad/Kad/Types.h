//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#ifndef TYPES_H
#define TYPES_H

#ifndef _MSC_VER
	#ifndef __STDC_FORMAT_MACROS
		#define __STDC_FORMAT_MACROS
	#endif
	#include <inttypes.h>
	#define LONGLONG(x) x##ll
	#define ULONGLONG(x) x##llu
#else
	typedef unsigned __int8 byte;
	typedef unsigned __int8 uint8_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int64 uint64_t;
	typedef signed __int8 int8_t;
	typedef signed __int16 int16_t;
	typedef signed __int32 int32_t;
	typedef signed __int64 int64_t;
	typedef unsigned long ULONG;
	#define LONGLONG(x) x##i64
	#define ULONGLONG(x) x##ui64
#endif

// These are _MSC_VER defines used in eMule. They should 
// not be used in aMule, instead, use this table to 
// find the type to use in order to get the desired 
// effect. 
//////////////////////////////////////////////////
// Name              // Type To Use In Amule    //
//////////////////////////////////////////////////
// BOOL              // bool                    //
// WORD              // uint16                  //
// INT               // int32                   //
// UINT              // uint32                  //
// UINT_PTR          // uint32*                 //
// PUINT             // uint32*                 //
// DWORD             // uint32                  //
// LONG              // long                    //
// ULONG             // unsigned long           //
// LONGLONG          // long long               //
// ULONGLONG         // unsigned long long      //
// LPBYTE            // char*                   //
// VOID              // void                    //
// PVOID             // void*                   //
// LPVOID            // void*                   //
// LPCVOID           // const void*             //
// CHAR              // char                    //
// LPSTR             // char*                   //
// LPCSTR            // const char*             //
// TCHAR             // char                    //
// LPTSTR            // char*                   //
// LPCTSTR           // const char*             //
// WCHAR             // wchar_t                 //
// LPWSTR            // wchar_t*                //
// LPCWSTR           // const wchar_t*          //
// WPARAM            // uint16                  //
// LPARAM            // uint32                  //
// POINT             // wxPoint                 //
//////////////////////////////////////////////////

#endif /* TYPES_H */
// File_checked_for_headers

/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.
Standard Windows declarations and converter assertions.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>

extern "C" int Assert_MyHandler( const char *expression, const char *file, int line );

#define assert( exp ) \
	do \
	{ \
		static bool s_ignore; \
		if ( !( exp ) && !s_ignore ) \
		{ \
			switch ( Assert_MyHandler( #exp, __FILE__, __LINE__ ) ) \
			{ \
			case 0: \
				__debugbreak(); \
				break; \
			case 2: \
				s_ignore = true; \
				break; \
			} \
		} \
	} while ( 0 )

#endif

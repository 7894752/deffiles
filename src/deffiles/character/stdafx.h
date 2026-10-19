/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Common includes and assertion support for the character converter.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../convert_value.h"

#define ARRAY_COUNT( array ) ( sizeof( array ) / sizeof( (array)[0] ) )
// Shared scratch space for formatted script and error messages.
#define MAX_VA_STRING 32000

extern "C" int Assert_MyHandler( const char *expression, const char *file, int line );

#define assert( exp ) \
	do { \
		static bool s_ignore; \
		if ( !( exp ) && !s_ignore ) { \
			switch ( Assert_MyHandler( #exp, __FILE__, __LINE__ ) ) { \
			case 0: \
				__asm { int 3 } \
				break; \
			case 2: \
				s_ignore = true; \
				break; \
			} \
		} \
	} while ( 0 )

#endif

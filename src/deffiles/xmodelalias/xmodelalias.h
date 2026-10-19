/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Model alias converter includes, limits and assertion support.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef XMODELALIAS_H
#define XMODELALIAS_H

#include <windows.h>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../convert_value.h"

// The .gdf defines model1 through model50.
#define MAX_XMODELALIAS 50
#define MAX_VA_STRING 32000

extern "C" int Assert_MyHandler( const char *expression, const char *file, int line );

#define assert( exp ) \
	do { \
		static bool s_ignore; \
		if ( !( exp ) && !s_ignore ) { \
			switch ( Assert_MyHandler( #exp, __FILE__, __LINE__ ) ) { \
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

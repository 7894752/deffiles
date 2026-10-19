/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef ASSERTIVE_H
#define ASSERTIVE_H

extern void ( *Assert_MessageHook )( const char *message );

int Assert_DoStackTrace( char *buffer, int maxFrames, int skipFrames );
extern "C" int Assert_MyHandler( const char *expression, const char *file, int line );

#define IW_ASSERT( expression ) \
	do { \
		static unsigned char s_bIgnoreAssert = 0; \
		if ( !( expression ) && !s_bIgnoreAssert ) { \
			switch ( Assert_MyHandler( #expression, __FILE__, __LINE__ ) ) { \
			case 0: __asm { int 3 } break; \
			case 2: s_bIgnoreAssert = 1; break; \
			} \
		} \
	} while ( 0 )

#endif

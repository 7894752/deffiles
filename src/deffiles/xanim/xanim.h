/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Animation export writer and vector utilities.

#ifndef XANIM_H
#define XANIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <string>

#include "xanim_model.h"

#define MAX_NOTETRACK 255

// Assertion handling.
extern "C" int Assert_MyHandler( const char *expression, const char *file, int line );

#define assert( x ) \
	do { \
		static bool ignoreAssert; \
		if ( !( x ) && !ignoreAssert ) { \
			switch ( Assert_MyHandler( #x, __FILE__, __LINE__ ) ) { \
			case 0: \
				__asm { int 3 } \
				break; \
			case 2: \
				ignoreAssert = true; \
				break; \
			} \
		} \
	} while ( 0 )

extern "C" {
FILE *SafeOpenWrite( const char *filename );
void SafeWrite( FILE *f, const void *buffer, int count );
float VectorNormalize4D( float *q );
float VectorDistanceSquared( const float *a, const float *b );
float VectorNormalize2D( float *v );
void MatrixTranspose( const float in[3][3], float out[3][3] );
void MatrixMultiply( const float in1[3][3], const float in2[3][3], float out[3][3] );
void MatrixTransformVector( const float v[3], const float m[3][3], float out[3] );
void MatrixTransformVector43( const float v[3], const float m[4][3], float out[3] );
void Swap_Init();
float AngleEigenTrace( float degrees );
float QuatRatioEigenTrace( const float *a, const float *b );
short LittleShort( short l );
float LittleFloat( float l );
}

struct noteTrack_t {
	char name[NOTETRACK_NAME_LEN];
	int frameIndex;
};

#endif

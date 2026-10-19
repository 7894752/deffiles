/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Default converter for GDF entries that do not specify a #dll.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include "stdafx.h"
#include <stdio.h>
#include "../convert_value.h"

/*
=============
DllMain
=============
*/
BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, LPVOID reserved ) {
	return TRUE;
}

/*
=============
DumpValue
=============
*/
static void DumpValue( FILE *f, ConvertValue *value ) {
	const char *key;
	ConvertValue *child;

	for ( key = value->GetNextKey( 0 ); key; key = value->GetNextKey( key ) ) {
		fprintf( f, "\"%s\" ", key );
		child = value->GetValue( key );
		if ( child->IsPrimType() ) {
			fprintf( f, "\"%s\"\n", child->GetString() );
		} else {
			fprintf( f, "\n{\n" );
			DumpValue( f, child );
			fprintf( f, "}\n" );
		}
	}
}

static char targetPath[1024];
static char sourcePath[1024];

/*
=============
Convert
=============
*/
int Convert( ConvertValue *cv ) {
	char buffer[1024];
	FILE *in, *out;

	sprintf( targetPath, "%s%s.txt", cv->GetTargetDirectory( 0 ), cv->GetName() );
	cv->AddTargetFile( targetPath );
	out = fopen( targetPath, "wb" );
	if ( !out ) {
		printf( "Cannot open '%s'\n", targetPath );
		return fflush( stdout );
	}

	DumpValue( out, cv );

	sprintf( sourcePath, "%s%s.dat", cv->GetSourceDirectory(), cv->GetName() );
	cv->AddSourceFile( sourcePath );
	in = fopen( sourcePath, "rb" );
	if ( in ) {
		// Append only the first whitespace-delimited token from the optional .dat file.
		fscanf( in, "%s", buffer );
		fprintf( out, "%s", buffer );
		fclose( in );
	}
	fclose( out );
	return fflush( stdout );
}

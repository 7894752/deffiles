/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Writes GDF entries as configstring files for weapons, info, player models and vehicles.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
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
WriteConfigString
=============
*/
static int WriteConfigString( FILE *f, ConvertValue *cv ) {
	char buffer[1024];
	const char *key;
	ConvertValue *value;
	int i, length;

	value = cv->GetValue( "configstringFileType" );
	if ( !value ) {
		printf( "To use the confgistring converter, you must specify a configstringFileType key for the file type identifier.\n" );
		return 0;
	}

	strcpy( buffer, value->GetString() );
	fprintf( f, buffer );

	for ( key = cv->GetNextKey( 0 ); key; key = cv->GetNextKey( key ) ) {
		// These keys control the output file rather than describing the asset.
		if ( !strcmp( key, "configstringFileType" ) || !strcmp( key, "targetFolder" ) ) {
			continue;
		}

		fprintf( f, "\\%s", key );
		value = cv->GetValue( key );
		if ( !value->IsPrimType() ) {
			printf( "Non-PrimType key value found. Not valid in a configstring format file.\n" );
			return 0;
		}

		strcpy( buffer, value->GetString() );
		length = strlen( buffer );
		// Replace characters that conflict with configstring and command syntax.
		for ( i = 0; i < length; i++ ) {
			if ( buffer[i] == '\\' ) {
				buffer[i] = '/';
			} else if ( buffer[i] == '"' ) {
				buffer[i] = '\'';
			} else if ( buffer[i] == ';' ) {
				buffer[i] = ':';
			}
		}
		fprintf( f, "\\%s", buffer );
	}

	return 1;
}

static char filename[1024];

/*
=============
Convert
=============
*/
int Convert( ConvertValue *cv ) {
	char folder[1024];
	ConvertValue *value;
	int index;
	FILE *f;

	index = 0;
	value = cv->GetValue( "targetFolder" );
	if ( value ) {
		strcpy( folder, value->GetString() );
		// Only the first character selects the target directory.
		index = folder[0] - '1';
		if ( index < 0 || index > 9 ) {
			printf( "WARNING: targetFolder '%s' out of range. First character should be a number to specify the folder\n", folder );
			fflush( stdout );
			return 1;
		}
		if ( !cv->GetTargetDirectory( index ) ) {
			printf( "WARNING: targetFolder '%s' out of range.\n", folder );
			fflush( stdout );
			return 1;
		}
	}

	sprintf( filename, "%s%s", cv->GetTargetDirectory( index ), cv->GetName() );
	cv->AddTargetFile( filename );
	f = fopen( filename, "wb" );
	if ( !f ) {
		printf( "Cannot open '%s'\n", filename );
		fflush( stdout );
		return 1;
	}

	if ( !WriteConfigString( f, cv ) ) {
		fclose( f );
		fflush( stdout );
		return 1;
	}

	fclose( f );
	fflush( stdout );
	return 0;
}

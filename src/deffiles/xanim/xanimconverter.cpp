/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Asset-manager entry point and model-match tables.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../convert_value.h"
#include "xanim_model.h"

extern "C" void Com_BeginParseSession( const char *filename );
extern "C" void Com_EndParseSession();
extern "C" void Com_SetCSV( int enabled );
extern "C" char *Com_Parse( char **text );
extern "C" char *Com_ParseOnLine( char **text );
extern "C" void Com_SkipRestOfLine( char **text );
extern "C" int FS_ReadFile( const char *filename, void **buffer );
extern "C" void FS_FreeFile( void *buffer );
extern "C" char *va( const char *format, ... );

static AnimationOptions options;
static bool modelMatchesLoaded;
extern "C" int conversionError;

extern "C" void Com_Error(int code, const char *fmt, ...);

#include "../shared/files.h"

/*
=============
DllMain
=============
*/
extern "C" int __stdcall DllMain( void *instance, unsigned long reason, void *reserved ) {
	return 1;
}

/*
=============
LoadModelMatches
=============
*/
void LoadModelMatches( ConvertValue *cv ) {
	char filename[1024];
	char error[1024];
	void *buffer;
	sprintf( filename, "%s%s", cv->GetSourceDirectory(), "source_data/modelmatch.csv" );
	cv->AddSourceFile( filename );
	if ( !modelMatchesLoaded ) {
		modelMatchesLoaded = true;
		if ( FS_ReadFile( filename, &buffer ) < 0 ) {
			throw va( "could not find '%s'", filename );
		}
		Com_BeginParseSession( filename );
		Com_SetCSV( true );
		char *text = (char *)buffer;
		bool first = true;
		while ( 1 ) {
			char *name = Com_ParseOnLine( &text );
			if ( !text ) break;
			if ( !name[0] ) {
				Com_SkipRestOfLine( &text );
				first = true;
				continue;
			}
			sprintf( filename, "%smodel_export/%s", cv->GetSourceDirectory(), name );
			try {
				LoadQuatBones( filename, first );
			} catch ( char *message ) {
				Com_EndParseSession();
				free( buffer );
				sprintf( error, "Error in '%s':\n%s", "source_data/modelmatch.csv", message );
				throw (char *)error;
			}
			first = false;
		}
		Com_EndParseSession();
		free( buffer );
	}
}

/*
=============
Convert
=============
*/
extern "C" __declspec(dllexport) int Convert( ConvertValue *cv ) {
	char type[1024];
	char filename[1024];
	char *buffer;
	const char *modelName = 0;
	try {
		const char *animName = cv->GetValue( "filename" )->GetString();
		if ( !animName[0] ) Error( "filename was not specified" );
		sprintf( options.animFilename, "%sxanim_export/%s", cv->GetSourceDirectory(), animName );
		printf( "Converting xanim '%s'...\n", options.animFilename );
		const char *assetName = cv->GetName();
		sprintf( options.outputFilename, "%s%s", cv->GetTargetDirectory( 0 ), assetName );
		options.looping = cv->GetValue( "looping" )->GetInteger() != 0;
		options.useBones = cv->GetValue( "useBones" )->GetInteger() != 0;
		strcpy( type, cv->GetValue( "type" )->GetString() );
		if ( !stricmp( type, "absolute" ) ) options.type = 0;
		else if ( !stricmp( type, "relative" ) ) options.type = 1;
		else if ( !stricmp( type, "delta" ) ) options.type = 2;
		else if ( !stricmp( type, "mp_torso" ) ) options.type = 3;
		else if ( !stricmp( type, "mp_legs" ) ) options.type = 4;
		else if ( !stricmp( type, "mp_fullbody" ) ) options.type = 5;
		else Error( "bad anim type '%s' for '%s'", type, cv->GetName() );
		strcpy( options.node, cv->GetValue( "node" )->GetString() );
		options.rotationThreshold = cv->GetValue( "angleError" )->GetFloat();
		if ( options.rotationThreshold < 0 ) Error( "negative angleError not allowed" );
		options.translationThreshold = cv->GetValue( "translationError" )->GetFloat();
		if ( options.translationThreshold < 0 ) Error( "negative translationError not allowed" );
		strcpy( options.stabilizers, cv->GetValue( "boneStabilizers" )->GetString() );
		if ( options.type == 3 || options.type == 4 || options.type == 5 ) {
			if ( options.type == 3 || options.type == 5 ) {
				sprintf( filename, "%s%s", cv->GetSourceDirectory(), "source_data/multiplayerstabilizers.csv" );
				cv->AddSourceFile( filename );
				if ( FS_ReadFile( filename, (void **)&buffer ) < 0 ) {
					throw va( "could not find '%s'", filename );
				}
				Com_BeginParseSession( filename );
				Com_SetCSV( true );
				char *text = buffer;
				while ( 1 ) {
					char *bone = Com_Parse( &text );
					if ( !text ) break;
					strcat( options.stabilizers, va( " \"%s\"", bone ) );
					bone = Com_Parse( &text );
					if ( !text ) {
						Com_EndParseSession();
						free( buffer );
						throw va( "error in reading '%s'", filename );
					}
					strcat( options.stabilizers, va( " \"%s\"", bone ) );
				}
				Com_EndParseSession();
				free( buffer );
			}
			switch ( options.type ) {
			case 3: modelName = "character_models\\mp\\torso.XMODEL_EXPORT"; options.type = 1; break;
			case 4: modelName = "character_models\\mp\\legs.XMODEL_EXPORT"; options.type = 1; break;
			case 5: modelName = "character_models\\mp\\fullbody.XMODEL_EXPORT"; options.type = 2; break;
			}
		}
		if ( !modelName ) {
			modelName = cv->GetValue( "model" )->GetString();
			if ( !modelName[0] ) Error( "model was not specified" );
		}
		sprintf( options.modelFilename, "%smodel_export/%s", cv->GetSourceDirectory(), modelName );
		cv->AddSourceFile( options.modelFilename );
		cv->AddSourceFile( options.animFilename );
		cv->AddTargetFile( options.outputFilename );
		LoadModelMatches( cv );
		ConvertAnimation( &options );
		fflush( stdout );
		return 0;
	} catch ( char *message ) {
		printf( "\n************ ERROR ************\n" );
		printf( "%s\n", message );
		fflush( stdout );
	}
	return 1;
}

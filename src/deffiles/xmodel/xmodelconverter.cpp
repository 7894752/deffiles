/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include "../convert_value.h"
#include "skelmodel.h"
#include "shaderlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

enum {
	MODELTYPE_RIGID,
	MODELTYPE_ANIMATED,
	MODELTYPE_VIEWMODEL,
	MODELTYPE_MULTIPLAYER_BODY,
	MODELTYPE_VIEWHANDS,
	MODELTYPE_COUNT
};

extern "C" {
char *va( const char *fmt, ... );
void Com_BeginParseSession( const char *name );
void Com_EndParseSession();
void Com_SetCSV( int csv );
char *Com_Parse( char **text );
int FS_ReadFile( const char *name, void **buffer );
int Assert_MyHandler( const char *expression, const char *file, int line );
char *COM_Parse( char *text );
extern char com_token[1024];

}
extern "C" void Error( const char *fmt, ... );

// Hit location names stored with model bones.
static const char *hitLocationNames[] = {
	"none", "helmet", "head", "neck", "torso_upper", "torso_lower",
	"right_arm_upper", "left_arm_upper", "right_arm_lower", "left_arm_lower",
	"right_hand", "left_hand", "right_leg_upper", "left_leg_upper",
	"right_leg_lower", "left_leg_lower", "right_foot", "left_foot", "gun"
};

PartClassification partClassifications[MAX_CLASSIFICATION] = {0};
CollModel collmodel = {0};
ConvertValue *convertValue = 0;
static bool classificationsLoaded = false;
int numPartClassifications = 0;

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
ExtractFileBase
=============
*/
static void ExtractFileBase( const char *path, char *dest ) {
	const char *p = path + strlen( path ) - 1;
	while ( p >= path ) {
		if ( *p == '/' || *p == '\\' ) {
			strcpy( dest, p + 1 );
			return;
		}
		p--;
	}
	*dest = 0;
}

/*
=============
StripExtension
=============
*/
static void StripExtension( char *path ) {
	char *p = path + strlen( path ) - 1;
	while ( p >= path ) {
		if ( *p == '.' ) {
			*p = 0;
			return;
		}
		p--;
	}
}

#include "../shared/files.h"

/*
=============
GetHitLocation
=============
*/
static int GetHitLocation( const char *name ) {
	for ( unsigned int i = 0; i < sizeof( hitLocationNames ) / sizeof( hitLocationNames[0] ); i++ ) {
		if ( !stricmp( name, hitLocationNames[i] ) ) return i;
	}
	throw va( "unknown hit location '%s'", name );
}

/*
=============
LoadPartClassifications
=============
*/
void LoadPartClassifications( ConvertValue *cv ) {
	char filename[1024];
	char *buffer, *text, *token;
	sprintf( filename, "%s%s", cv->GetSourceDirectory(), "source_data/partclassification.csv" );
	cv->AddSourceFile( filename );
	if ( !classificationsLoaded ) {
		classificationsLoaded = true;
		if ( FS_ReadFile( filename, (void **)&buffer ) < 0 ) throw va( "could not find '%s'", filename );
		Com_BeginParseSession( filename );
		Com_SetCSV( 1 );
		text = buffer;
		while ( 1 ) {
			if ( numPartClassifications >= MAX_CLASSIFICATION ) throw "MAX_CLASSIFICATION exceeded";
			token = Com_Parse( &text );
			if ( !text ) break;
			strcpy( partClassifications[numPartClassifications].name, token );
			token = Com_Parse( &text );
			if ( !text ) {
				Com_EndParseSession();
				free( buffer );
				throw va( "error in reading '%s'", filename );
			}
			partClassifications[numPartClassifications].classification = GetHitLocation( token );
			numPartClassifications++;
		}
		Com_EndParseSession();
		free( buffer );
	}
}

extern void BuildModel( ModelLods *lods, int lod, const char *source, const char *parts,
	const char *sourceDir, const char *gameDir, const char *skinDir,
	bool writeOffsets, const char *surfs, const char *controllers, const char *stabilizers,
	BoneNames *bones, bool radialNormals, const float *radialOrigin, int animated,
	const char *rigidRoot, SurfaceNames *surfNames, SkinOverride *overrides, int overrideCount,
	bool viewmodel, const char *hitBox );

#line 194
/*
=============
ConvertModelLod
=============
*/
static void ConvertModelLod( ConvertValue *cv, ModelLods *lods, int lod, bool writeParts,
	const char *controllers, const char *stabilizers, BoneNames *bones, bool radialNormals,
	const float *radialOrigin, bool writeOffsets, int type, const char *rigidRoot,
	SurfaceNames *surfNames, SkinOverride *overrides, int overrideCount, const char *hitBox ) {
	char source[1024], sourceDir[1024], parts[1024], surfs[1024], hitBoxFile[1024];
	char partsDir[1024], surfsDir[1024], skinDir[1024], baseName[1024], gameDir[1024];
	char *filename = lods->lods[lod].filename;
	assert( filename[0] );
	sprintf( source, "%smodel_export/%s", cv->GetSourceDirectory(), filename );
	strcpy( partsDir, cv->GetTargetDirectory( 2 ) );
	ExtractFileBase( filename, baseName );
	StripExtension( baseName );
	strcpy( filename, baseName );
	strcat( filename, va( "%i", type ) );
	sprintf( parts, "%s%s", partsDir, filename );
	strcpy( sourceDir, source );
	char *p = sourceDir + strlen( sourceDir ) - 1;
	char ch;
	while ( (ch = *p) != '/' && ch != '\\' ) p--;
	p[1] = 0;
	shaderLibrary.cv = cv;
	sprintf( shaderLibrary.dir, "%s%s", cv->GetSourceRootDirectory(), cv->GetTargetRootDirectory() );
	sprintf( gameDir, "%s%s", cv->GetSourceRootDirectory(), cv->GetTargetRootDirectory() );
	// The host supplies skins as the second relative target directory.
	strcpy( skinDir, cv->GetRelativeTargetDirectory( 1 ) );
	strcpy( surfsDir, cv->GetTargetDirectory( 3 ) );
	sprintf( surfs, "%s%s", surfsDir, filename );
	printf( "Converting xmodel '%s'...\n", source );
	cv->AddSourceFile( source );
	if ( writeParts ) cv->AddTargetFile( parts );
	cv->AddTargetFile( surfs );
	convertValue = cv;
	int animated = type != MODELTYPE_RIGID;
	bool viewmodel = type == MODELTYPE_VIEWHANDS;
	if ( writeParts && hitBox[0] ) sprintf( hitBoxFile, "%smodel_export/%s", cv->GetSourceDirectory(), hitBox );
	else hitBoxFile[0] = 0;
	BuildModel( lods, lod, source, writeParts ? parts : NULL, sourceDir, gameDir, skinDir,
		writeOffsets, surfs, controllers, stabilizers, bones, radialNormals, radialOrigin,
		animated, rigidRoot, surfNames, overrides, overrideCount, viewmodel, hitBoxFile );
}

/*
=============
ParseSkinOverrides
=============
*/
static int ParseSkinOverrides( char *text, SkinOverride *overrides ) {
	int count = 0;
	text = COM_Parse( text );
	while ( text ) {
		if ( count >= MAX_SKIN_OVERRIDE ) Error( "MAX_SKIN_OVERRIDE exceeded" );
		memset( overrides[count].material, 0, 64 );
		strncpy( overrides[count].material, com_token, 63 );
		overrides[count].used = false;
		text = COM_Parse( text );
		if ( !text ) Error( "no skin override specified for %s", overrides[count].material );
		memset( overrides[count].object, 0, 64 );
		if ( !strcmp( com_token, "(" ) ) {
			text = COM_Parse( text );
			if ( !text ) Error( "no object for skin override specified for %s", overrides[count].material );
			strncpy( overrides[count].object, com_token, 63 );
			text = COM_Parse( text );
			if ( !text || strcmp( com_token, ")" ) ) Error( "expecting ')' for skin override of '%s'", overrides[count].material );
			text = COM_Parse( text );
			if ( !text ) Error( "no skin override specified for %s", overrides[count].material );
		}
		memset( overrides[count].skin, 0, 64 );
		strncpy( overrides[count].skin, com_token, 63 );
		count++;
		text = COM_Parse( text );
	}
	return count;
}

/*
=============
FreeSurfaceNames
=============
*/
static void FreeSurfaceNames( SurfaceNames *lists ) {
	for ( int lod = 0; lod < 3; lod++ ) {
		for ( int i = 0; i < lists[lod].num; i++ ) free( lists[lod].names[i] );
		lists[lod].num = 0;
	}
}

extern CollModel collmodel;
extern void WriteXModel( const char *filename, ModelLods *lods, SurfaceNames *surfNames );
extern "C" void Printf( const char *fmt, ... );

static const char *modelTypeNames[] = { "rigid", "animated", "viewmodel", "multiplayer body", "viewhands" };

#line 357
/*
=============
Convert
=============
*/
extern "C" __declspec(dllexport) int Convert( ConvertValue *cv ) {
	SurfaceNames surfNames[3];
	BoneNames *highBones, *lowBones;
	try {
		ModelLods lods;
		char rigidRoots[3][1024];
		char collisionLod[1024];
		float radialOrigin[3];
		char controllers[2048], stabilizers[2048], skinText[4096];
		char filename[1024], *buffer, *text;
		char targetDir[1024], outputName[1024];
		SkinOverride overrides[MAX_SKIN_OVERRIDE];
		char hitBox[1024];
		for ( int i = 0; i < 3; i++ ) surfNames[i].num = 0;
		LoadPartClassifications( cv );
		highBones = new BoneNames;
		lowBones = new BoneNames;
		strcpy( lods.lods[0].filename, cv->GetValue( "filename" )->GetString() );
		strcpy( lods.lods[1].filename, cv->GetValue( "mediumLod" )->GetString() );
		strcpy( lods.lods[2].filename, cv->GetValue( "lowLod" )->GetString() );
		const char *typeName = cv->GetValue( "type" )->GetString();
		int iModelType[3];
		for ( iModelType[0] = MODELTYPE_RIGID; iModelType[0] < MODELTYPE_COUNT; iModelType[0]++ ) {
			if ( !stricmp( modelTypeNames[iModelType[0]], typeName ) ) break;
		}
		if ( iModelType[0] == MODELTYPE_COUNT ) throw "bad type";
		strcpy( rigidRoots[0], cv->GetValue( "highLodRigidRoot" )->GetString() );
		bool forceRigid = cv->GetValue( "forceMediumLodRigid" )->GetInteger() != 0;
		iModelType[1] = forceRigid ? MODELTYPE_RIGID : iModelType[0];
		strcpy( rigidRoots[1], cv->GetValue( "mediumLodRigidRoot" )->GetString() );
		if ( iModelType[0] == MODELTYPE_RIGID ) {
			assert( iModelType[1] == MODELTYPE_RIGID );
			if ( stricmp( rigidRoots[1], rigidRoots[0] ) ) {
				if ( lods.lods[1].filename[0] ) Printf( "*** WARNING: Overriding mediumLodRigidRoot '%s' with '%s'\n", rigidRoots[1], rigidRoots[0] );
				strcpy( rigidRoots[1], rigidRoots[0] );
			}
		}
		forceRigid = cv->GetValue( "forceLowLodRigid" )->GetInteger() != 0;
		iModelType[2] = forceRigid ? MODELTYPE_RIGID : iModelType[1];
		strcpy( rigidRoots[2], cv->GetValue( "lowLodRigidRoot" )->GetString() );
		if ( iModelType[1] == MODELTYPE_RIGID ) {
#line 408
			assert( iModelType[2] == MODELTYPE_RIGID );
			if ( stricmp( rigidRoots[2], rigidRoots[1] ) ) {
				if ( lods.lods[2].filename[0] ) Printf( "*** WARNING: Overriding lowLodRigidRoot '%s' with '%s'\n", rigidRoots[2], rigidRoots[1] );
				strcpy( rigidRoots[2], rigidRoots[1] );
			}
		}
		bool writeOffsets = iModelType[0] != MODELTYPE_VIEWMODEL && iModelType[0] != MODELTYPE_VIEWHANDS;
		lods.lods[0].distance = cv->GetValue( "highLodDist" )->GetFloat();
		lods.lods[1].distance = cv->GetValue( "mediumLodDist" )->GetFloat();
		lods.lods[2].distance = cv->GetValue( "lowLodDist" )->GetFloat();
		if ( cv->GetValue( "calculateBounds" )->GetInteger() ) {
			lods.mins[0] = 1; lods.mins[1] = 1; lods.mins[2] = 1;
			lods.maxs[0] = -1; lods.maxs[1] = -1; lods.maxs[2] = -1;
		} else {
			lods.mins[0] = cv->GetValue( "minX" )->GetFloat();
			lods.mins[1] = cv->GetValue( "minY" )->GetFloat();
			lods.mins[2] = cv->GetValue( "minZ" )->GetFloat();
			lods.maxs[0] = cv->GetValue( "maxX" )->GetFloat();
			lods.maxs[1] = cv->GetValue( "maxY" )->GetFloat();
			lods.maxs[2] = cv->GetValue( "maxZ" )->GetFloat();
		}
		strcpy( collisionLod, cv->GetValue( "collisionLOD" )->GetString() );
		memset( &collmodel, 0, sizeof( collmodel ) );
		lods.collisionLod = -1;
		if ( !strcmp( collisionLod, "High" ) ) lods.collisionLod = 0;
		else if ( !strcmp( collisionLod, "Medium" ) ) lods.collisionLod = 1;
		else if ( !strcmp( collisionLod, "Low" ) ) lods.collisionLod = 2;
		bool radialNormals = cv->GetValue( "noRadialNormals" )->GetInteger() == 0;
		if ( radialNormals ) {
			radialOrigin[0] = cv->GetValue( "radialOriginX" )->GetFloat();
			radialOrigin[1] = cv->GetValue( "radialOriginY" )->GetFloat();
			radialOrigin[2] = cv->GetValue( "radialOriginZ" )->GetFloat();
		}
		strcpy( controllers, cv->GetValue( "boneControllers" )->GetString() );
		strcpy( stabilizers, cv->GetValue( "boneStabilizers" )->GetString() );
		strcpy( skinText, cv->GetValue( "skinOverride" )->GetString() );
		if ( iModelType[0] == MODELTYPE_MULTIPLAYER_BODY ) {
			sprintf( filename, "%s%s", cv->GetSourceDirectory(), "source_data/multiplayercontrollers.csv" );
			cv->AddSourceFile( filename );
			if ( FS_ReadFile( filename, (void **)&buffer ) < 0 ) throw va( "could not find '%s'", filename );
			Com_BeginParseSession( filename );
			Com_SetCSV( 1 );
			text = buffer;
			while ( 1 ) {
				char *token = Com_Parse( &text );
				if ( !text ) break;
				strcat( controllers, va( " \"%s\"", token ) );
				token = Com_Parse( &text );
				if ( !text ) { Com_EndParseSession(); free( buffer ); throw va( "error in reading '%s'", filename ); }
				strcat( controllers, va( " \"%s\"", token ) );
			}
			Com_EndParseSession(); free( buffer );
			sprintf( filename, "%s%s", cv->GetSourceDirectory(), "source_data/multiplayerstabilizers.csv" );
			cv->AddSourceFile( filename );
			if ( FS_ReadFile( filename, (void **)&buffer ) < 0 ) throw va( "could not find '%s'", filename );
			Com_BeginParseSession( filename ); Com_SetCSV( 1 ); text = buffer;
			while ( 1 ) {
				char *token = Com_Parse( &text );
				if ( !text ) break;
				strcat( stabilizers, va( " \"%s\"", token ) );
				token = Com_Parse( &text );
				if ( !text ) { Com_EndParseSession(); free( buffer ); throw va( "error in reading '%s'", filename ); }
				strcat( stabilizers, va( " \"%s\"", token ) );
			}
			Com_EndParseSession(); free( buffer );
		}
		strcpy( targetDir, cv->GetTargetDirectory( 0 ) );
		sprintf( outputName, "%s%s", targetDir, cv->GetName() );
		cv->AddTargetFile( outputName );
		if ( !lods.lods[0].filename[0] ) throw "no filename specified";
		for ( i = 0; i < 3; i++ ) {
			if ( lods.lods[i].distance < 0 ) throw "negative LOD distance not allowed";
			if ( lods.lods[i].distance != 0 ) {
				if ( !lods.lods[i].filename[0] ) throw "LOD distance without a filename is not allowed";
			} else if ( !lods.lods[i].filename[0] && lods.collisionLod >= i ) throw "collision LOD is not valid";
		}
		int overrideCount = ParseSkinOverrides( skinText, overrides );
		strcpy( hitBox, cv->GetValue( "hitBoxModel" )->GetString() );
		ConvertModelLod( cv, &lods, 0, true, controllers, stabilizers, highBones, radialNormals,
			radialOrigin, writeOffsets, iModelType[0], rigidRoots[0], &surfNames[0], overrides, overrideCount, hitBox );
		for ( i = 1; i < 3; i++ ) {
			if ( !lods.lods[i].filename[0] ) continue;
			if ( !lods.lods[i-1].filename[0] ) throw va( "missing LOD above '%s'", lods.lods[i].filename );
			if ( (lods.lods[i].distance != 0 && lods.lods[i].distance <= lods.lods[i-1].distance) || lods.lods[i-1].distance == 0 )
				throw va( "LOD '%s' does not have greater distance than LOD '%s'", lods.lods[i].filename, lods.lods[i-1].filename );
			ConvertModelLod( cv, &lods, i, false, controllers, stabilizers, lowBones, radialNormals,
				radialOrigin, writeOffsets, iModelType[i], rigidRoots[i], &surfNames[i], overrides, overrideCount, NULL );
			if ( lowBones->count > highBones->count ) throw va( "Lower lod '%s' has more parts than '%s'", lods.lods[i].filename, lods.lods[0].filename );
			for ( int j = 1; j < lowBones->count; j++ ) {
				if ( stricmp( lowBones->names[j], highBones->names[j] ) ) throw va( "Part '%s' in lower lod '%s' does not have the same name as the part '%s' in higher lod '%s'", lowBones->names[j], lods.lods[i].filename, highBones->names[j], lods.lods[0].filename );
				if ( lowBones->parents[j] != highBones->parents[j] ) throw va( "Part '%s' (with parent '%s') in lower lod '%s' does not have the same parent as the matching part (with parent '%s') in higher lod '%s'", lods.lods[i].filename, lowBones->parents[j] != -1 ? lowBones->names[lowBones->parents[j]] : "<none>", lowBones->names[j], highBones->parents[j] != -1 ? highBones->names[highBones->parents[j]] : "<none>", lods.lods[0].filename );
			}
		}
		for ( i = 0; i < overrideCount; i++ ) {
			if ( !overrides[i].used ) Error( "original skin '%s' (object '%s') specified by skinOverride not used", overrides[i].material, overrides[i].object );
		}
		WriteXModel( outputName, &lods, surfNames );
		delete highBones; delete lowBones;
		FreeSurfaceNames( surfNames );
		fflush( stdout );
		return 0;
	} catch ( char *message ) {
		printf( "\n************ ERROR ************\n" );
		printf( "%s\n", message );
		delete highBones; delete lowBones;
		FreeSurfaceNames( surfNames );
		fflush( stdout );
	}
	return 1;
}

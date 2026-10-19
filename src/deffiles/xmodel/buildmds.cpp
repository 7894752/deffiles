/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Read and build skeletal models, render surfaces and collision surfaces.

extern "C" char com_token[];
extern "C" const char *COM_Parse( const char * );
extern "C" char *strlower( char * );

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "skelmodel.h"
#include "animationdata.h"
#include <malloc.h>
#include <wchar.h>
#include <ctype.h>
#include "shaderlib.h"
extern CollModel collmodel;
#include "common/q_round.h"

extern "C" {
float VectorDistance( const float *, const float * );
float VectorNormalize( float * );
int VectorCompareEpsilon( const float *, const float * );
}
bool VertsMatch( const ModelVert *, const ModelVert *, int );
extern "C" int PlaneFromPoints( float *, const float *, const float *, const float * );

#include <windows.h>
#include <new>
#include <stdlib.h>
#include "../convert_value.h"
#include "script.h"

#define VectorSubtract( a, b, c ) ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
#define VectorScale( v, s, o ) ( ( o )[0] = ( v )[0] * ( s ), ( o )[1] = ( v )[1] * ( s ), ( o )[2] = ( v )[2] * ( s ) )
#define VectorMA( v, s, b, o ) ( ( o )[0] = ( v )[0] + ( s ) * ( b )[0], ( o )[1] = ( v )[1] + ( s ) * ( b )[1], ( o )[2] = ( v )[2] + ( s ) * ( b )[2] )
#define DotProduct( a, b ) ( ( a )[0] * ( b )[0] + ( a )[1] * ( b )[1] + ( a )[2] * ( b )[2] )
#define VectorClear( a ) ( ( a )[0] = ( a )[1] = ( a )[2] = 0 )
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )

extern "C" {
void AddPointToBounds( const float *v, float *mins, float *maxs );
float VectorDistanceSquared( const float *p1, const float *p2 );

void Com_BeginParseSession( const char *name );
void Com_EndParseSession();
char *Com_Parse( char **data_p );
void Com_UngetToken();
int Com_SkipBracedSection( char **data_p, int startDepth );
void Com_SkipRestOfLine( char **data_p );
}

extern "C" int LoadFile( const char *filename, void **buffer );




ShaderLibrary shaderLibrary;



/*
=============
Animation::Init
=============
*/
void Animation::Init() {
	memset( &model, 0, offsetof( AnimationData, file ) );
	model.numControllers = 0;
	model.numStabilizers = 0;
}

/*
=============
Animation::Free
=============
*/
void Animation::Free() {
	int i;

	for ( i = 0; i < model.numframes; i++ ) {
		delete [] model.frames[i].bones;
	}
	delete [] model.nodeFrames;
	delete [] model.vertRemap;
	delete [] model.verts;
	delete [] model.vertsSaved;
	delete [] model.faces;
	delete [] model.frames;
	delete [] model.bones;
	delete [] model.fileToModel;
	delete [] model.surfaces;
	delete [] model.objectNames;
	delete [] model.materialNames;
	delete [] model.animToModel;
	memset( &model, 0, offsetof( AnimationData, file ) );
}

/*
=============
SkelModel::Init
=============
*/
void SkelModel::Init() {
	memset( &model, 0, sizeof( model ) );
	numControllers = 0;
	numStabilizers = 0;
}

/*
=============
SkelModel::Free
=============
*/
void SkelModel::Free() {
	int i;

	for ( i = 0; i < model.numframes; i++ ) {
		delete[] model.poses[i].pose;
	}
	delete[] model.nodeFrames;
	delete[] model.vertRemap;
	delete[] model.verts;
	delete[] model.vertsSaved;
	delete[] model.faces;
	delete[] model.poses;
	delete[] model.bones;
	delete[] model.boneRemap;
	delete[] model.surfaces;
	delete[] model.objectNames;
	delete[] model.materialNames;
	delete[] model.animToModel;
	memset( &model, 0, sizeof( model ) );
}

/*
=============
SkelModel::LoadBoneControllers
=============
*/
void SkelModel::LoadBoneControllers( const char *text ) {
	numControllers = 0;
	while ( ( text = COM_Parse( text ) ) != 0 ) {
		if ( numControllers >= 8 ) {
			Error( "maximum bone controllers exceeded" );
		}
		strcpy( controllerNames[numControllers], com_token );
		strlower( controllerNames[numControllers] );
		text = COM_Parse( text );
		if ( !text ) {
			Error( "no bone controller name specified for %s", controllerNames[numControllers] );
		}
		strcpy( controllerTargets[numControllers], com_token );
		strlower( controllerTargets[numControllers] );
		numControllers++;
	}
}

/*
=============
SkelModel::LoadBoneStabilizers
=============
*/
void SkelModel::LoadBoneStabilizers( const char *text ) {
	numStabilizers = 0;
	while ( ( text = COM_Parse( text ) ) != 0 ) {
		if ( numStabilizers >= 2 ) {
			Error( "maximum bone stabilizers exceeded" );
		}
		strcpy( stabilizerNames[numStabilizers], com_token );
		strlower( stabilizerNames[numStabilizers] );
		text = COM_Parse( text );
		if ( !text ) {
			Error( "no bone stabilizer name specified for %s", stabilizerNames[numStabilizers] );
		}
		strcpy( stabilizerTargets[numStabilizers], com_token );
		strlower( stabilizerTargets[numStabilizers] );
		numStabilizers++;
	}
}

/*
=============
SkelModel::LoadObjects
=============
*/
void SkelModel::LoadObjects( Script *script ) {
	script->MatchToken( "NUMOBJECTS", true );
	int count = script->GetInteger( false );
	model.objectNames = new char[count][64];
	memset( model.objectNames, 0, count * sizeof( *model.objectNames ) );
	for ( model.numObjects = 0; count > 0; count-- ) {
		script->MatchToken( "OBJECT", true );
		char *object = model.objectNames[model.numObjects];
		script->GetInteger( false );
		memset( object, 0, sizeof( model.objectNames[0] ) );
		strncpy( object, script->GetToken( false ), sizeof( model.objectNames[0] ) - 1 );
		model.numObjects++;
	}
}

/*
=============
FindImage
=============
*/
static MaterialName *FindImage( MaterialName *skins, int count, const char *name ) {
	int i = 0;
	if ( count > 0 ) {
		MaterialName *skin = skins;
		do {
			if ( !stricmp( name, skin->name ) ) {
				return skin;
			}
			i++;
			skin++;
		} while ( i < count );
	}
	return 0;
}

/*
=============
SkelModel::FindImage
=============
*/
MaterialName *SkelModel::FindImage( const char *name ) {
	return ::FindImage( model.materialNames, model.numMaterials, name );
}

#line 345 "buildmds.cpp"
/*
=============
Animation::LoadBones
=============
*/
void Animation::LoadBones( Script &script ) {
	int i, j, k;
	int parent, number;
	Bone *bone;
	char name[MODEL_NAME_LEN];
	script.MatchToken( "NUMBONES", true );
	model.numFileBones = script.GetInteger( false );
	model.numbones = model.numStabilizers + model.numControllers + model.numFileBones;
	if ( model.numbones > MAX_BONES ) {
		script.Error( "Too many bones in model.  Numbones (including bone controllers & bone stabilizers) cannot exceed %d.", MAX_BONES );
	}
	model.bones = new Bone[model.numbones];
	model.fileToModel = new int[model.numFileBones];
	for ( i = 0; i < model.numFileBones; i++ ) {
		model.fileToModel[i] = i;
	}
	memset( model.controllerUsed, 0, sizeof( model.controllerUsed ) );
	memset( model.stabilizerUsed, 0, sizeof( model.stabilizerUsed ) );
	bone = model.bones;
	for ( i = 0; i < model.numFileBones; i++, bone++ ) {
		script.MatchToken( "BONE", true );
		number = script.GetInteger( false );
		if ( number != i ) {
			script.Error( "Bone number %i out of sync", number );
		}
		parent = script.GetInteger( false );
		if ( parent < -1 || parent >= model.numFileBones ) {
			script.Error( "Parent bone out of range for bone %i", number );
		}
		if ( parent != -1 ) {
			parent = model.fileToModel[parent];
		}
		strncpy( name, script.GetString( false ), MODEL_NAME_LEN - 1 );
		strlower( name );
		for ( j = 0; j < model.numStabilizers; j++ ) {
			if ( !stricmp( name, model.stabilizerBone[j] ) ) {
				if ( parent == -1 ) {
					script.Error( "Bone stabilizers not allowed on root bone %s", model.stabilizerBone[j] );
				}
				bone->fileIndex = model.fileToModel[i];
				bone->parent = parent;
				bone->set = false;
				memset( bone->name, 0, MODEL_NAME_LEN );
				strncpy( bone->name, model.stabilizerName[j], MODEL_NAME_LEN - 1 );
				bone->stabilizer = 1;
				model.stabilizerUsed[j] = 1;
				for ( k = i; k < model.numFileBones; k++ ) {
					model.fileToModel[k]++;
				}
				parent = bone->fileIndex;
				bone++;
				for ( k = j + 1; k < model.numStabilizers; k++ ) {
					if ( !stricmp( bone->name, model.stabilizerBone[k] ) ) {
						script.Error( "More than one bone stabilizers on %s", model.stabilizerBone[k] );
					}
				}
				break;
			}
		}
		bone->fileIndex = model.fileToModel[i];
		bone->parent = parent;
		bone->set = false;
		bone->stabilizer = 0;
		memset( bone->name, 0, MODEL_NAME_LEN );
		strncpy( bone->name, name, MODEL_NAME_LEN - 1 );
		for ( j = 0; j < model.numControllers; j++ ) {
			if ( !stricmp( bone->name, model.controllerBone[j] ) ) {
				for ( k = i; k < model.numFileBones; k++ ) {
					model.fileToModel[k]++;
				}
				bone++;
				bone->fileIndex = model.fileToModel[i];
				bone->set = false;
				bone->parent = bone->fileIndex - 1;
				assert( (bone->parent >= 0) && (bone->parent < model.numbones) );

				bone->stabilizer = 0;
				memset( bone->name, 0, MODEL_NAME_LEN );
				strncpy( bone->name, model.controllerName[j], MODEL_NAME_LEN - 1 );
				strlower( bone->name );
				model.controllerUsed[j] = 1;
				for ( k = j + 1; k < model.numControllers; k++ ) {
					if ( !stricmp( bone->name, model.controllerBone[k] ) ) {
						script.Error( "More than one bone controller on %s", model.controllerBone[k] );
					}
				}
				break;
			}
		}
	}


	// Every requested helper must name an exported bone.
	for ( i = 0; i < model.numControllers; i++ ) {
		if ( !model.controllerUsed[i] ) {
			Error( "bone '%s' for bone controller '%s' not found", model.controllerBone[i], model.controllerName[i] );
		}
	}

	for ( i = 0; i < model.numStabilizers; i++ ) {
		if ( !model.stabilizerUsed[i] ) {
			Error( "bone '%s' for bone stabilizer '%s' not found", model.stabilizerBone[i], model.stabilizerName[i] );
		}
	}

	// Controllers and stabilizers also participate in name lookup.
	// Reject collisions after all of them have been inserted.
	bone = model.bones;
	for ( i = 0; i < model.numbones; i++, bone++ ) {
		for ( j = i + 1; j < model.numbones; j++ ) {
			if ( !stricmp( bone->name, model.bones[j].name ) ) {
				Error( "duplicate bone '%s'", bone->name );
			}
		}
	}
}

#line 340
/*
=============
SkelModel::LoadBones
=============
*/
void SkelModel::LoadBones( Script *script ) {
	int i, j, k;
	int parent, number;
	ModelBone *bone;
	char name[64];

	script->MatchToken( "NUMBONES", true );
	model.numRootBones = script->GetInteger( false );
	model.numbones = numStabilizers + numControllers + model.numRootBones;
	if ( model.numbones > 256 ) {
		script->Error( "Too many bones in model.  Numbones (including bone controllers & bone stabilizers) cannot exceed %d.", 256 );
	}
	model.bones = new ModelBone[model.numbones];
	model.boneRemap = new int[model.numRootBones];
	for ( i = 0; i < model.numRootBones; i++ ) {
		model.boneRemap[i] = i;
	}
	memset( controllerUsed, 0, sizeof( controllerUsed ) );
	memset( stabilizerUsed, 0, sizeof( stabilizerUsed ) );

	bone = model.bones;
	for ( i = 0; i < model.numRootBones; i++, bone++ ) {
		script->MatchToken( "BONE", true );
		number = script->GetInteger( false );
		if ( number != i ) {
			script->Error( "Bone number %i out of sync", number );
		}
		parent = script->GetInteger( false );
		if ( parent < -1 || parent >= model.numRootBones ) {
			script->Error( "Parent bone out of range for bone %i", number );
		}
		if ( parent != -1 ) {
			parent = model.boneRemap[parent];
		}
		strncpy( name, script->GetString( false ), 63 );
		strlower( name );

		for ( j = 0; j < numStabilizers; j++ ) {
			if ( !stricmp( name, stabilizerNames[j] ) ) {
				if ( parent == -1 ) {
					script->Error( "Bone stabilizers not allowed on root bone %s", stabilizerNames[j] );
				}
				bone->index = model.boneRemap[i];
				bone->parent = parent;
				bone->set = false;
				memset( bone->name, 0, 64 );
				strncpy( bone->name, stabilizerTargets[j], 63 );
				bone->stabilizer = 1;
				stabilizerUsed[j] = 1;
				for ( k = i; k < model.numRootBones; k++ ) {
					model.boneRemap[k]++;
				}
				parent = bone->index;
				bone++;
				for ( k = j + 1; k < numStabilizers; k++ ) {
					if ( !stricmp( bone->name, stabilizerNames[k] ) ) {
						script->Error( "More than one bone stabilizers on %s", stabilizerNames[k] );
					}
				}
				break;
			}
		}

		bone->index = model.boneRemap[i];
		bone->parent = parent;
		bone->set = false;
		bone->stabilizer = 0;
		memset( bone->name, 0, 64 );
		strncpy( bone->name, name, 63 );

		for ( j = 0; j < numControllers; j++ ) {
			if ( !stricmp( bone->name, controllerNames[j] ) ) {
				for ( k = i; k < model.numRootBones; k++ ) {
					model.boneRemap[k]++;
				}
				bone++;
				bone->index = model.boneRemap[i];
				bone->set = false;
				bone->parent = bone->index - 1;
				assert( (bone->parent >= 0) && (bone->parent < model.numbones) );
				bone->stabilizer = 0;
				memset( bone->name, 0, 64 );
				strncpy( bone->name, controllerTargets[j], 63 );
				strlower( bone->name );
				controllerUsed[j] = 1;
				for ( k = j + 1; k < numControllers; k++ ) {
					if ( !stricmp( bone->name, controllerNames[k] ) ) {
						script->Error( "More than one bone controller on %s", controllerNames[k] );
					}
				}
				break;
			}
		}
	}

	for ( i = 0; i < numControllers; i++ ) {
		if ( !controllerUsed[i] ) {
			Error( "bone '%s' for bone controller '%s' not found", controllerNames[i], controllerTargets[i] );
		}
	}
	for ( i = 0; i < numStabilizers; i++ ) {
		if ( !stabilizerUsed[i] ) {
			Error( "bone '%s' for bone stabilizer '%s' not found", stabilizerNames[i], stabilizerTargets[i] );
		}
	}
	bone = model.bones;
	for ( i = 0; i < model.numbones; i++, bone++ ) {
		for ( j = i + 1; j < model.numbones; j++ ) {
			if ( !stricmp( bone->name, model.bones[j].name ) ) {
				Error( "duplicate bone '%s'", bone->name );
			}
		}
	}
}

/*
=============
VertsMatch
=============
*/
bool VertsMatch( const ModelVert *a, const ModelVert *b, int tolerant ) {
	const Blend *blendA, *blendB;
	float tolerance;
	int i, j;

	if ( a->numbones != b->numbones ) {
		return false;
	}
	if ( !tolerant ) {
		tolerance = 0;
	} else if ( a->maxEdgeLength > b->maxEdgeLength ) {
		tolerance = a->maxEdgeLength;
	} else {
		tolerance = b->maxEdgeLength;
	}
	if ( VectorDistanceSquared( a->pos, b->pos ) > tolerance * tolerance * 0.0001f ) {
		return false;
	}
	blendA = a->blend;
	for ( i = 0; i < a->numbones; i++, blendA++ ) {
		blendB = b->blend;
		for ( j = 0; j < b->numbones; j++, blendB++ ) {
			if ( blendA->bone == blendB->bone ) {
				if ( fabs( blendA->weight - blendB->weight ) < 0.01f ) {
					break;
				}
				return false;
			}
		}
		if ( j == b->numbones ) {
			return false;
		}
	}
	return true;
}

/*
=============
SkelModel::FindVertex
=============
*/
int SkelModel::FindVertex( const ModelVert *vert ) {
	int i;

	for ( i = 0; i < model.numverts; i++ ) {
		ModelVert *other = &model.verts[i];

		if ( VertsMatch( vert, other, vert->merged || other->merged ) ) {
			return i;
		}
	}
	return model.numverts;
}

/*
=============
SkelModel::ReadFaces
=============
*/
void SkelModel::ReadFaces( Script *script ) {
	ModelFace *face;
	FaceCorner *corner;
	int i, j;

	script->MatchToken( "NUMFACES", true );
	model.numfaces = script->GetInteger( false );
	model.faces = new ModelFace[model.numfaces];
	face = model.faces;
	for ( i = 0; i < model.numfaces; i++, face++ ) {
		script->MatchToken( "TRI", true );
		face->object = script->GetInteger( false );
		face->material = script->GetInteger( false );
		if ( face->material == -1 ) {
			script->Error( "Face with no material - xmodel needs to be re-exported" );
		}
		face->unused = script->GetInteger( false );
		face->bUsed = false;
		script->GetInteger( false );
		corner = face->corners;
		for ( j = 0; j < 3; j++, corner++ ) {
			int index;

			script->MatchToken( "VERT", true );
			index = script->GetInteger( false );
			if ( index < 0 || index >= model.numSourceVerts ) {
				script->Error( "Vertex index out of range" );
			}
			corner->index = index;
			corner->st[0] = script->GetFloat( false );
			corner->st[1] = script->GetFloat( false );
			corner->normal[0] = script->GetFloat( false );
			corner->normal[1] = script->GetFloat( false );
			corner->normal[2] = script->GetFloat( false );
			VectorNormalize( corner->normal );
		}
	}
}

/*
=============
Animation::FindFrame
=============
*/
int Animation::FindFrame( int number ) {
	int i;

	for ( i = model.numframes - 1; i >= 0; i-- ) {
		if ( model.frames[i].number == number ) {
			break;
		}
	}
	return i;
}

#line 1040 "buildmds.cpp"
/*
=============
Animation::LoadBasePose
=============
*/
void Animation::LoadBasePose( Script &script ) {
	BoneFrame *originBone = 0;
	model.numframes = 1;
	model.frames = new Frame[1];
	model.frames[0].bones = new BoneFrame[model.numbones];
	for ( int j = 0; j < model.numbones; j++ ) {
		model.frames[0].bones[j].set = 0;
	}
	for ( int k = 0; k < model.numFileBones; k++ ) {
		script.MatchToken( "BONE", true );
		if ( script.GetInteger( false ) != k ) {
			script.Error( "Bone number out of sync" );
		}
		BoneFrame *bone = &model.frames[0].bones[model.fileToModel[k]];
		bone->set = 1;
		script.MatchToken( "OFFSET", true );
		script.GetVector( bone->offset, false );
		script.MatchToken( "SCALE", true );
		script.GetVector( bone->scale, false );
		script.MatchToken( "X", true );
		script.GetVector( bone->axis[0], false );
		script.MatchToken( "Y", true );
		script.GetVector( bone->axis[1], false );
		script.MatchToken( "Z", true );
		script.GetVector( bone->axis[2], false );
	}
	int i;
	if ( model.numStabilizers ) {
		for ( i = 0; i < model.numbones; i++ ) {
			if ( !stricmp( model.bones[i].name, "tag_origin" ) ) {
				originBone = &model.frames[0].bones[i];
				break;
			}
		}
		if ( !originBone ) {
			script.Error( "Model does not have a tag_origin bone" );
		}
	}
	for ( i = 0; i < model.numbones; i++ ) {
		BoneFrame *bone = &model.frames[0].bones[i];
		if ( !bone->set ) {
			assert( i != model.numbones - 1 );
			if ( model.bones[i].stabilizer ) {
				BoneFrame *parent = &model.frames[0].bones[model.bones[i].parent];
				VectorCopy( parent->offset, bone->offset );
				VectorCopy( parent->scale, bone->scale );
				AxisCopy( originBone->axis, bone->axis );
			} else {
				BoneFrame *next = &model.frames[0].bones[i + 1];
				VectorCopy( next->offset, bone->offset );
				VectorCopy( next->scale, bone->scale );
				AxisCopy( next->axis, bone->axis );
			}
		}
	}
}

#line 1036
/*
=============
SkelModel::ReadBasePose
=============
*/
void SkelModel::ReadBasePose( Script *script ) {
	BonePose *pose;
	BonePose *tagPose = 0;
	int i;

	model.numframes = 1;
	model.poses = new BonePoses[1];
	model.poses[0].pose = new BonePose[model.numbones];
	for ( i = 0; i < model.numbones; i++ ) {
		model.poses[0].pose[i].set = 0;
	}
	for ( i = 0; i < model.numRootBones; i++ ) {
		script->MatchToken( "BONE", true );
		if ( script->GetInteger( false ) != i ) {
			script->Error( "Bone number out of sync" );
		}
		pose = &model.poses[0].pose[model.boneRemap[i]];
		pose->set = 1;
		script->MatchToken( "OFFSET", true );
		script->GetVector( pose->origin, false );
		script->MatchToken( "SCALE", true );
		script->GetVector( pose->scale, false );
		script->MatchToken( "X", true );
		script->GetVector( pose->axis[0], false );
		script->MatchToken( "Y", true );
		script->GetVector( pose->axis[1], false );
		script->MatchToken( "Z", true );
		script->GetVector( pose->axis[2], false );
	}
	if ( numStabilizers ) {
		for ( i = 0; i < model.numbones; i++ ) {
			if ( !stricmp( model.bones[i].name, "tag_origin" ) ) {
				tagPose = &model.poses[0].pose[i];
				break;
			}
		}
		if ( !tagPose ) {
			script->Error( "Model does not have a tag_origin bone" );
		}
	}
	for ( i = 0; i < model.numbones; i++ ) {
		pose = &model.poses[0].pose[i];
		if ( pose->set ) {
			continue;
		}
		assert( i != model.numbones - 1 );
		if ( model.bones[i].stabilizer ) {
			BonePose *parentPose = &model.poses[0].pose[model.bones[i].parent];

			VectorCopy( parentPose->origin, pose->origin );
			VectorCopy( parentPose->scale, pose->scale );
			VectorCopy( tagPose->axis[0], pose->axis[0] );
			VectorCopy( tagPose->axis[1], pose->axis[1] );
			VectorCopy( tagPose->axis[2], pose->axis[2] );
		} else {
			BonePose *nextPose = &model.poses[0].pose[i + 1];

			VectorCopy( nextPose->origin, pose->origin );
			VectorCopy( nextPose->scale, pose->scale );
			VectorCopy( nextPose->axis[0], pose->axis[0] );
			VectorCopy( nextPose->axis[1], pose->axis[1] );
			VectorCopy( nextPose->axis[2], pose->axis[2] );
		}
	}
}

/*
=============
SkelModel::VertsEqual
=============
*/
bool SkelModel::VertsEqual( int a, int b ) {
	float tolerance;
	int i;

	if ( a == b ) {
		return true;
	}
	if ( model.verts[a].numbones != model.verts[b].numbones ) {
		return false;
	}
	if ( model.verts[a].numbones == 1 ) {
		tolerance = 0.2f * 0.2f;
	} else {
		tolerance = 0.001f * 0.001f;
	}
	if ( VectorDistanceSquared( model.verts[a].pos, model.verts[b].pos ) > tolerance ) {
		return false;
	}
	for ( i = 0; i < model.verts[a].numbones; i++ ) {
		if ( model.verts[a].blend[i].bone != model.verts[b].blend[i].bone || model.verts[a].blend[i].weight != model.verts[b].blend[i].weight ) {
			return false;
		}
	}
	return true;
}

/*
=============
SkelModel::AddSurfaceVert
=============
*/
int SkelModel::AddSurfaceVert( ModelSurface *surf, FaceCorner *corner, int object, int material ) {
	SurfVert *surfVert;
	vec3_t normal;
	float tolerance, normalTolerance;
	int vertIndex, bone;
	int i;
	float st[2];
	float delta[2];

	vertIndex = corner->index;
	st[0] = corner->st[0];
	st[1] = corner->st[1];
	if ( model.materialNames[material].radialNormals ) {
		normal[0] = model.verts[vertIndex].pos[0] - radialOrigin[0];
		normal[1] = model.verts[vertIndex].pos[1] - radialOrigin[1];
		normal[2] = model.verts[vertIndex].pos[2] - radialOrigin[2];
		VectorNormalize( normal );
	} else {
		normal[0] = corner->normal[0];
		normal[1] = corner->normal[1];
		normal[2] = corner->normal[2];
	}
	if ( !viewmodel && model.verts[vertIndex].numbones == 1 ) {
		bone = model.verts[vertIndex].blend[0].bone;
	} else {
		bone = -1;
	}
	if ( model.verts[vertIndex].merged ) {
		tolerance = 0.01f;
		normalTolerance = 0.01f;
	} else {
		tolerance = 1e-5f;
		normalTolerance = 1.0f / 255;
	}
	surfVert = surf->verts;
	for ( i = 0; i < surf->numverts; i++, surfVert++ ) {
		delta[0] = fabs( surfVert->st[0] - st[0] );
		delta[1] = fabs( surfVert->st[1] - st[1] );

		if ( VertsEqual( surfVert->index, vertIndex ) && delta[0] <= tolerance && delta[1] <= tolerance
			&& VectorDistance( surfVert->normal, normal ) <= normalTolerance ) {
			if ( surfVert->bone != bone ) {
				surfVert->bone = -1;
			}
			return i;
		}
	}
	if ( surf->numverts >= MAX_SURF_VERTS ) {
		Error( "Exceeded %d vertices on surface '%s' of object '%s'\n", MAX_SURF_VERTS, model.materialNames[material].name, model.objectNames[object] );
	}
	surfVert->index = vertIndex;
	surfVert->bone = bone;
	surfVert->st[0] = st[0];
	surfVert->st[1] = st[1];
	surfVert->normal[0] = normal[0];
	surfVert->normal[1] = normal[1];
	surfVert->normal[2] = normal[2];
	surf->numverts++;
	return i;
}

/*
=============
SkelModel::AddSurfaceTri
=============
*/
int SkelModel::AddSurfaceTri( ModelSurface *surf, FaceCorner *corners, int object, int material ) {
	int numtris, numverts;
	int *index;
	int i;
	FaceCorner *corner;

	if ( surf->numtris >= MAX_SURF_TRIS ) {
		Error( "Exceeded %d triangles on surface '%s' of object '%s'\n", MAX_SURF_TRIS, model.materialNames[material].name, model.objectNames[object] );
	}
	numtris = surf->numtris;
	numverts = surf->numverts;
	index = surf->tris[numtris].index;
	for ( corner = corners, i = 0; i < 3; i++, corner++ ) {
		index[i] = AddSurfaceVert( surf, corner, object, material );
	}
	if ( index[0] == index[1] || index[0] == index[2] || index[1] == index[2] ) {
		surf->numverts = numverts;
		return -1;
	}
	surf->numtris++;
	return numtris;
}

/*
=============
ModelSurface::ResolveVertexBones
=============
*/
int ModelSurface::ResolveVertexBones() {
	int changed = 0;
	int i;

	for ( i = 0; i < numtris; i++ ) {
		if ( verts[tris[i].index[0]].bone != verts[tris[i].index[1]].bone || verts[tris[i].index[0]].bone != verts[tris[i].index[2]].bone ) {
			verts[tris[i].index[0]].bone = -1;
			verts[tris[i].index[1]].bone = -1;
			verts[tris[i].index[2]].bone = -1;
			changed = 1;
		}
	}
	return changed;
}

/*
=============
SkelModel::AddSurface
=============
*/
ModelSurface *SkelModel::AddSurface( int object, int material, int bone ) {
	ModelSurface *surfaces;
	ModelSurface *surf;
	int i;

	if ( model.numsurfaces == model.maxsurfaces ) {
		model.maxsurfaces *= 2;
		surfaces = new ModelSurface[model.maxsurfaces];
		memset( surfaces, 0, model.maxsurfaces * sizeof( ModelSurface ) );
		for ( i = 0; i < model.numsurfaces; i++ ) {
			surfaces[i] = model.surfaces[i];
		}
		delete[] model.surfaces;
		model.surfaces = surfaces;
	}
	surf = &model.surfaces[model.numsurfaces];
	surf->object = object;
	surf->numtris = 0;
	surf->numverts = 0;
	surf->material = material;
	surf->boneIndex = bone;
	model.numsurfaces++;
	return surf;
}

/*
=============
SkelModel::CalculateBounds
=============
*/
void SkelModel::CalculateBounds( ModelInfo *info ) {
	vec3_t mins, maxs, size, delta, local;
	float inverse[3][3];
	BonePose *pose;
	float radius, length, diagonal;
	int i;

	if ( model.numverts == 3 ) {
		VectorClear( info->mins );
		VectorClear( info->maxs );
		return;
	}
	if ( model.numbones > 1 ) {
		printf( "*** WARNING: Calculating bounds on a model with more than 1 bone\n" );
	}
	pose = model.poses[0].pose;
	mins[0] = FLT_MAX;
	mins[1] = FLT_MAX;
	mins[2] = FLT_MAX;
	maxs[0] = -FLT_MAX;
	maxs[1] = -FLT_MAX;
	maxs[2] = -FLT_MAX;
	radius = 0;
	for ( i = 0; i < model.numverts; i++ ) {
		delta[0] = model.verts[i].pos[0] - pose->origin[0];
		delta[1] = model.verts[i].pos[1] - pose->origin[1];
		delta[2] = model.verts[i].pos[2] - pose->origin[2];
		MatrixTranspose( pose->axis, inverse );
		MatrixTransformVector( delta, inverse, local );
		AddPointToBounds( local, mins, maxs );
		length = sqrt( local[0] * local[0] + local[1] * local[1] + local[2] * local[2] );
		if ( radius < length ) {
			radius = length;
		}
	}
	size[0] = maxs[0] - mins[0];
	size[1] = maxs[1] - mins[1];
	size[2] = maxs[2] - mins[2];
	if ( radius > 32 ) {
		diagonal = sqrt( size[0] * size[0] + size[1] * size[1] + size[2] * size[2] );
		if ( diagonal * 1.732f < radius ) {
			printf( "actual mins/maxs (%g %g %g) (%g %g %g)\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2] );
			printf( "actual size (%g %g %g) radius %g\n", size[0], size[1], size[2], diagonal * 0.5f );
			printf( "rotation invariant radius %g\n", radius );
			Error( "auto-generated rotation invariant bounds far exceed physical bounds; model should be exported closer to the root bone\n" );
		}
	}
	info->mins[0] = -radius;
	info->mins[1] = -radius;
	info->mins[2] = -radius;
	info->maxs[0] = radius;
	info->maxs[1] = radius;
	info->maxs[2] = radius;
}

/*
=============
ShaderLibrary::ShaderLibrary
=============
*/
ShaderLibrary::ShaderLibrary() {
	cv = 0;
	types = 0;
	text = 0;
	files = 0;
	loaded = false;
}

/*
=============
ShaderLibrary::~ShaderLibrary
=============
*/
ShaderLibrary::~ShaderLibrary() {
	ShaderType *type;
	ShaderFile *file;

	while ( types ) {
		type = types;
		types = type->next;
		free( type->text );
		free( type );
	}
	while ( files ) {
		file = files;
		files = file->next;
		free( file );
	}
	free( text );
	loaded = false;
}

/*
=============
ShaderLibrary::GetShaderType
=============
*/
char *ShaderLibrary::GetShaderType( const char *name ) {
	char path[1024];
	ShaderType *type;

	sprintf( path, "%sshadertypes/model/%s.stype", dir, name );
	for ( type = types; type; type = type->next ) {
		if ( !stricmp( type->name, name ) ) {
			cv->AddSourceFile( path );
			return type->text;
		}
	}
	type = (ShaderType *)malloc( sizeof( ShaderType ) );
	strcpy( type->name, name );
	type->next = types;
	types = type;
	if ( LoadFile( path, (void **)&type->text ) < 0 ) {
		printf( "*** WARNING: Can't find shader type [%s] ***\n", path );
		type->text = 0;
	}
	cv->AddSourceFile( path );
	return type->text;
}

/*
=============
ShaderLibrary::LoadShaderFiles
=============
*/
char *ShaderLibrary::LoadShaderFiles() {
	char pattern[1024];
	WIN32_FIND_DATA findData;
	HANDLE handle;
	ShaderFile *file;
	FILE *f;
	int size, offset;

	if ( loaded ) {
		return text;
	}
	loaded = true;
	sprintf( pattern, "%s%s", dir, "scripts/*.shader" );
	handle = FindFirstFile( pattern, &findData );
	if ( handle == INVALID_HANDLE_VALUE ) {
		return 0;
	}
	size = 1;
	do {
		file = (ShaderFile *)malloc( sizeof( ShaderFile ) );
		if ( !file ) {
			return 0;
		}
		sprintf( file->path, "%s%s%s", dir, "scripts/", findData.cFileName );
		file->next = files;
		files = file;
		f = fopen( file->path, "rb" );
		fseek( f, 0, SEEK_END );
		int length = ftell( f );
		size += length + 1;
		fclose( f );
	} while ( FindNextFile( handle, &findData ) );
	FindClose( handle );
	text = (char *)malloc( size );
	if ( !text ) {
		return 0;
	}
	offset = 0;
	for ( file = files; file; file = file->next ) {
		f = fopen( file->path, "rb" );
		fseek( f, 0, SEEK_END );
		int length = ftell( f );
		fseek( f, 0, SEEK_SET );
		if ( offset + length >= size ) {
			Error( "error filling shader text buffer... shader file sizes grew between finding and reading files\n" );
		}
		fread( text + offset, 1, length, f );
		fclose( f );
		offset += length;
		text[offset++] = '\n';
	}
	text[offset] = 0;
	return text;
}

#line 1590
/*
=============
ShaderLibrary::FindShader
=============
*/
char *ShaderLibrary::FindShader( const char *name ) {
	char buffer[1024];
	char *at, *shaderText, *token;
	ShaderFile *file;
	int num;

	at = strrchr( name, '@' );
	if ( at ) {
		num = at - name;
		assert( num > 0 );
		strncpy( buffer, name, num );
		buffer[num] = 0;
		return GetShaderType( buffer );
	}
	shaderText = LoadShaderFiles();
	if ( !shaderText ) {
		printf( "*** WARNING: Can't find any shader files ***\n" );
		return 0;
	}
	Com_BeginParseSession( "shaders" );
	token = Com_Parse( &shaderText );
	while ( *token ) {
		Com_SkipRestOfLine( &shaderText );
		if ( !stricmp( token, name ) ) {
			for ( file = files; file; file = file->next ) {
				cv->AddSourceFile( file->path );
			}
			Com_EndParseSession();
			return shaderText;
		}
		Com_SkipBracedSection( &shaderText, 0x7fffffff );
		token = Com_Parse( &shaderText );
	}
	Com_EndParseSession();
	return 0;
}

/*
=============
SkelModel::ParseShaderSurfaceParms
=============
*/
void SkelModel::ParseShaderSurfaceParms( const char *shaderName, int *contentFlags, int *surfaceFlags ) {
	char *shaderText, *token, *parm;
	int i;

	*contentFlags = CONTENTS_SOLID;
	*surfaceFlags = 0;
	shaderText = shaderLibrary.FindShader( shaderName );
	if ( !shaderText ) {
		printf( "*** WARNING: Can't find shader [%s] for collision surface ***\n", shaderName );
		return;
	}
	token = Com_Parse( &shaderText );
	if ( strcmp( token, "{" ) ) {
		printf( "*** WARNING: Missing '{' at start of shader [%s] ***\n", shaderName );
		return;
	}
	Com_SkipRestOfLine( &shaderText );
	while ( ( token = Com_Parse( &shaderText ) ) && *token ) {
		if ( !strcmp( token, "}" ) ) {
			break;
		}
		if ( !strcmp( token, "{" ) ) {
			Com_UngetToken();
			Com_SkipBracedSection( &shaderText, 0x7fffffff );
			continue;
		}
		if ( !strcmp( token, "surfaceparm" ) ) {
			parm = Com_Parse( &shaderText );
			for ( i = 0; infoParms[i].name; i++ ) {
				if ( !stricmp( parm, infoParms[i].name ) ) {
					if ( infoParms[i].clearSolid ) {
						*contentFlags &= ~CONTENTS_SOLID;
					}
					*contentFlags |= infoParms[i].contents;
					*surfaceFlags |= infoParms[i].surfaceFlags;
					break;
				}
			}
			if ( !infoParms[i].name ) {
				if ( stricmp( parm, "castshadow" ) && stricmp( parm, "noshadow" ) && strnicmp( parm, "qer", 3 ) ) {
					printf( "*** WARNING: Could not find [%s] surfaceparm\n", parm );
				}
			}
		}
	}
}

#line 1719
/*
=============
SkelModel::BuildCollisionModel
=============
*/
void SkelModel::BuildCollisionModel( CollModel &collmodel ) {
	float axis[3][3], offset[3], point[3], point1[3], point2[3], edge1[3], edge2[3];
	collmodel.numCollSurfs = model.numsurfaces;
	assert( collmodel.numCollSurfs <= MAX_COLL_SURFS );
	for ( int i = 0; i < collmodel.numCollSurfs; i++ ) {
		CollSurface *csurf = &collmodel.collSurfs[i];
		ModelSurface *surf = &model.surfaces[i];
		if ( surf->boneIndex == -1 ) {
			Error( "Collision surf must be rigid bound\n" );
			return;
		}
		csurf->boneIdx = surf->boneIndex;
		csurf->mins[0] = FLT_MAX;
		csurf->mins[1] = FLT_MAX;
		csurf->mins[2] = FLT_MAX;
		csurf->maxs[0] = -FLT_MAX;
		csurf->maxs[1] = -FLT_MAX;
		csurf->maxs[2] = -FLT_MAX;
#line 1748
		assert( surf->numverts );
		int j;
		for ( j = 0; j < surf->numverts; j++ ) {
			ModelVert *vert = &model.verts[surf->verts[j].index];
			BonePose *pose = &model.poses[0].pose[vert->blend[0].bone];
			VectorSubtract( vert->pos, pose->origin, offset );
			MatrixTranspose( pose->axis, axis );
			MatrixTransformVector( offset, axis, point );
			if ( csurf->mins[0] > point[0] ) csurf->mins[0] = point[0];
			if ( csurf->maxs[0] < point[0] ) csurf->maxs[0] = point[0];
			if ( csurf->mins[1] > point[1] ) csurf->mins[1] = point[1];
			if ( csurf->maxs[1] < point[1] ) csurf->maxs[1] = point[1];
			if ( csurf->mins[2] > point[2] ) csurf->mins[2] = point[2];
			if ( csurf->maxs[2] < point[2] ) csurf->maxs[2] = point[2];
		}
		ParseShaderSurfaceParms( model.materialNames[surf->material].name, &csurf->contents, &csurf->surfaceFlags );
		csurf->numCollTris = surf->numtris;
#line 1774
		assert( csurf->numCollTris <= MAX_TRIANGLES );
#line 1776
		assert( csurf->numCollTris );
		for ( j = 0; j < csurf->numCollTris; j++ ) {
			CollTri *tri = &csurf->tris[j];
			{
				ModelVert *vert = &model.verts[surf->verts[surf->tris[j].index[0]].index];
				BonePose *pose = &model.poses[0].pose[vert->blend[0].bone];
				VectorSubtract( vert->pos, pose->origin, offset );
				MatrixTranspose( pose->axis, axis );
				MatrixTransformVector( offset, axis, point );
			}
			{
				ModelVert *vert = &model.verts[surf->verts[surf->tris[j].index[1]].index];
				BonePose *pose = &model.poses[0].pose[vert->blend[0].bone];
				VectorSubtract( vert->pos, pose->origin, offset );
				MatrixTranspose( pose->axis, axis );
				MatrixTransformVector( offset, axis, point1 );
			}
			{
				ModelVert *vert = &model.verts[surf->verts[surf->tris[j].index[2]].index];
				BonePose *pose = &model.poses[0].pose[vert->blend[0].bone];
				VectorSubtract( vert->pos, pose->origin, offset );
				MatrixTranspose( pose->axis, axis );
				MatrixTransformVector( offset, axis, point2 );
			}

			PlaneFromPoints( tri->plane, point, point1, point2 );
			VectorSubtract( point1, point, edge1 );
			VectorSubtract( point2, point, edge2 );
			float length = VectorNormalize( edge1 );
			float length2 = VectorNormalize( edge2 );
			float dot, scale;
			dot = DotProduct( edge1, edge2 );
			scale = 1 / ( 1 - dot * dot );
			VectorScale( edge1, scale, edge1 );
			VectorScale( edge2, scale, edge2 );

			VectorMA( edge1, -dot, edge2, tri->svec );
			VectorScale( tri->svec, 1 / length, tri->svec );
			tri->svec[3] = DotProduct( tri->svec, point );
			VectorMA( edge2, -dot, edge1, tri->tvec );
			VectorScale( tri->tvec, 1 / length2, tri->tvec );
			tri->tvec[3] = DotProduct( tri->tvec, point );
		}
	}
}

#line 1834
/*
=============
SkelModel::TransformNormalsToBones
=============
*/
void SkelModel::TransformNormalsToBones() {
	ModelSurface *surf = model.surfaces;
	float axis[3][3], normal[3];
	for ( int i = 0; i < model.numsurfaces; i++, surf++ ) {
		for ( int j = 0; j < surf->numverts; j++ ) {
			assert( model.verts[surf->verts[j].index].numbones > 0 );
			MatrixTranspose( model.poses[0].pose[model.verts[surf->verts[j].index].blend[0].bone].axis, axis );
			MatrixTransformVector( surf->verts[j].normal, axis, normal );
			surf->verts[j].normal[0] = normal[0];
			surf->verts[j].normal[1] = normal[1];
			surf->verts[j].normal[2] = normal[2];
		}
	}
}

/*
=============
SkelModel::CheckSurfaceTexCoords
=============
*/
void SkelModel::CheckSurfaceTexCoords() {
	ModelSurface *surf = model.surfaces;
	for ( int s = 0; s < model.numsurfaces; s++, surf++ ) {
		surf->state = 0;
		if ( !surf->numverts ) continue;
		int i, j;
		for ( i = 1; i < surf->numverts; i++ ) {
			for ( j = 0; j < i; j++ ) {
				if ( VectorCompareEpsilon( model.verts[surf->verts[i].index].pos, model.verts[surf->verts[j].index].pos ) ) {
					float delta[2];
					delta[0] = (float)fabs( surf->verts[i].st[0] - surf->verts[j].st[0] );
					delta[1] = (float)fabs( surf->verts[i].st[1] - surf->verts[j].st[1] );
					if ( delta[0] < 0.01f ) {
						if ( fabs( delta[1] - 1.0f ) < 0.01f ) {
							surf->state = 2;
							goto nextSurface;
						}
					} else if ( delta[1] < 0.01f ) {
						if ( fabs( delta[0] - 1.0f ) < 0.01f ) {
							surf->state = 2;
							goto nextSurface;
						}
					}
				}
			}
		}
		float mins[2], maxs[2];
		mins[0] = maxs[0] = surf->verts[0].st[0];
		mins[1] = maxs[1] = surf->verts[0].st[1];
		for ( i = 1; i < surf->numverts; i++ ) {
			if ( mins[0] > surf->verts[i].st[0] ) mins[0] = surf->verts[i].st[0];
			else if ( maxs[0] < surf->verts[i].st[0] ) maxs[0] = surf->verts[i].st[0];
			if ( mins[1] > surf->verts[i].st[1] ) mins[1] = surf->verts[i].st[1];
			else if ( maxs[1] < surf->verts[i].st[1] ) maxs[1] = surf->verts[i].st[1];
		}
		int floorU = FloorToInt( mins[0] ), floorV = FloorToInt( mins[1] );
		int ceilU = CeilToInt( maxs[0] ), ceilV = CeilToInt( maxs[1] );
		if ( ceilU <= floorU + 1 && ceilV <= floorV + 1 ) {
			if ( floorU || floorV ) {
				for ( i = 0; i < surf->numverts; i++ ) {
					surf->verts[i].st[0] -= floorU;
					surf->verts[i].st[1] -= floorV;
				}
			}
			continue;
		}
		int offsetU = FloorToInt( (mins[0] + maxs[0]) * 0.5f );
		int offsetV = FloorToInt( (mins[1] + maxs[1]) * 0.5f );
		if ( offsetU || offsetV ) {
			for ( i = 0; i < surf->numverts; i++ ) {
				surf->verts[i].st[0] -= offsetU;
				surf->verts[i].st[1] -= offsetV;
			}
			mins[0] -= offsetU;
			maxs[0] -= offsetU;
			mins[1] -= offsetV;
			maxs[1] -= offsetV;
		}
		if ( mins[0] >= -0.003f && maxs[0] <= 1.003f && mins[1] >= -0.003f && maxs[1] <= 1.003f ) {
			for ( i = 0; i < surf->numverts; i++ ) {
				if ( surf->verts[i].st[0] < 0 ) surf->verts[i].st[0] = 0;
				else if ( surf->verts[i].st[0] > 1 ) surf->verts[i].st[0] = 1;
				if ( surf->verts[i].st[1] < 0 ) surf->verts[i].st[1] = 0;
				else if ( surf->verts[i].st[1] > 1 ) surf->verts[i].st[1] = 1;
			}
		} else if ( mins[0] >= -0.05f && maxs[0] <= 1.05f && mins[1] >= -0.05f && maxs[1] <= 1.05f ) {
			printf( "*** WARNING: surface %s appears to be accidentally tiling, it probably has vertices too close to the edge\n", model.materialNames[surf->material].name );
			surf->state = 1;
		} else {
			surf->state = 2;
		}
	nextSurface: ;
	}
}

#line 1992
/*
=============
SkelModel::CheckVertexRoots
=============
*/
void SkelModel::CheckVertexRoots() {
	ModelVert *vert = model.verts;
	for ( int i = 0; i < model.numverts; i++, vert++ ) {
		assert( vert->numbones > 0 );
		Blend *blend = vert->blend;
		int root = blend->bone;
		while ( model.bones[root].parent >= 0 )
			root = model.bones[root].parent;
		blend++;
		for ( int j = 1; j < vert->numbones; j++, blend++ ) {
			int bone = blend->bone;
			while ( model.bones[bone].parent >= 0 )
				bone = model.bones[bone].parent;
			if ( root != bone )
				Error( "vertex %d attached to 2 different roots\n", i );
		}
	}
}

#line 2018
/*
=============
SkelModel::NormalizeBlends
=============
*/
void SkelModel::NormalizeBlends( ModelVert *vert, int vertNum ) {
	ModelVert in;
	float totalweight, maxweight;
	int i, j, k;

	if ( vert->numbones == 0 ) {
		Error( "No valid weights on vertex %d\n", vertNum );
	}
	in = *vert;
	totalweight = 0;
	for ( i = 0; i < in.numbones; i++ ) {
		totalweight += in.blend[i].weight;
	}
	assert( totalweight );
	if ( totalweight != 1 ) {
		for ( i = 0; i < in.numbones; i++ ) {
			in.blend[i].weight *= 1 / totalweight;
		}
	}
	vert->numbones = 0;
	totalweight = 0;
	for ( i = 0; i < in.numbones; i++ ) {
		for ( k = i + 1; k < in.numbones; k++ ) {
			if ( in.blend[i].bone == in.blend[k].bone ) {
				in.blend[i].weight += in.blend[k].weight;
				in.numbones--;
				in.blend[k] = in.blend[in.numbones];
			}
		}
	}
	for ( ;; ) {
		maxweight = 0;
		j = -1;
		for ( k = 0; k < in.numbones; k++ ) {
			if ( in.blend[k].bone != -1 && in.blend[k].weight > maxweight ) {
				j = k;
				maxweight = in.blend[k].weight;
			}
		}
		if ( maxweight == 0 ) {
			break;
		}
#line 2079
		assert( j != -1 );
		if ( maxweight < 0.01f ) {
			printf( "Bone %d for vertex %d ignored (weight %f too small)\n", in.blend[j].bone, vertNum, maxweight );
			in.blend[j].bone = -1;
			continue;
		}
		vert->blend[vert->numbones].bone = in.blend[j].bone;
		vert->blend[vert->numbones].weight = maxweight;
		in.blend[j].bone = -1;
		totalweight += maxweight;
		vert->numbones++;
		if ( vert->numbones >= 4 ) {
			for ( k = 0; k < in.numbones; k++ ) {
				if ( in.blend[k].bone != -1 ) {
					printf( "Bone %d for vertex %d ignored (max blend bones exceeded)\n", in.blend[k].bone, vertNum );
				}
			}
			break;
		}
	}
#line 2108
	assert( totalweight > 0 );
	assert( vert->numbones > 0 );
	if ( totalweight != 1 ) {
		for ( i = 0; i < vert->numbones; i++ ) {
			vert->blend[i].weight *= 1 / totalweight;
		}
	}
}

#line 2119
/*
=============
SkelModel::MakeRigid
=============
*/
void SkelModel::MakeRigid( const char *root ) {
	int bone;
	if ( !root[0] ) {
		assert( model.numbones > 0 );
		model.numbones = 1;
		model.bones[0].name[0] = 0;
		bone = 0;
	} else {
		for ( bone = 0; bone < model.numbones; bone++ ) {
			if ( !stricmp( model.bones[bone].name, root ) )
				break;
		}
		if ( bone == model.numbones )
			Error( "rigid root '%s' not found", root );
	}
	for ( int i = 0; i < model.numverts; i++ ) {
		ModelVert *vert = &model.verts[i];
#line 2147
		assert( vert->numbones > 0 );
		Blend &blend = vert->blend[0];
		vert->numbones = 1;
		blend.bone = bone;
		blend.weight = 1;
	}
}

/*
=============
SkelModel::ClassifyBones
=============
*/
void SkelModel::ClassifyBones() {
	for ( int i = 0; i < model.numbones; i++ ) {
		ModelBone *bone = &model.bones[i];
		int j;
		for ( j = 0; j < numPartClassifications; j++ ) {
			if ( !stricmp( bone->name, partClassifications[j].name ) )
				break;
		}
		if ( j != numPartClassifications )
			bone->classification = partClassifications[j].classification;
		else if ( bone->parent != -1 )
			bone->classification = model.bones[bone->parent].classification;
		else
			bone->classification = 0;
	}
}

/*
=============
SkelModel::UpdateEdgeLengths
=============
*/
void SkelModel::UpdateEdgeLengths( ModelVert *v1, ModelVert *v2 ) {
	float length = VectorDistance( v1->pos, v2->pos );
	if ( length > v1->maxEdgeLength )
		v1->maxEdgeLength = length;
	if ( length > v2->maxEdgeLength )
		v2->maxEdgeLength = length;
	if ( length < v1->minEdgeLength )
		v1->minEdgeLength = length;
	if ( length < v2->minEdgeLength )
		v2->minEdgeLength = length;
}

/*
=============
SkelModel::CalculateEdgeLengths
=============
*/
void SkelModel::CalculateEdgeLengths() {
	int i;
	for ( i = 0; i < model.numSourceVerts; i++ ) {
		model.verts[i].maxEdgeLength = 0;
		model.verts[i].minEdgeLength = FLT_MAX;
	}
	ModelFace *face = model.faces;
	for ( i = 0; i < model.numfaces; i++, face++ ) {
		UpdateEdgeLengths( &model.verts[face->corners[0].index], &model.verts[face->corners[1].index] );
		UpdateEdgeLengths( &model.verts[face->corners[0].index], &model.verts[face->corners[2].index] );
		UpdateEdgeLengths( &model.verts[face->corners[1].index], &model.verts[face->corners[2].index] );
	}
}

/*
=============
SkelModel::MarkSplitVertex
=============
*/
void SkelModel::MarkSplitVertex( int faceIndex, int vertexIndex ) {
	ModelVert *vert = &model.verts[model.faces[faceIndex].corners[vertexIndex].index];
	if ( vert->merged )
		return;
	int object = model.faces[faceIndex].object;
	for ( int i = 0; i < model.numfaces; i++ ) {
		ModelFace *face = &model.faces[i];
		if ( face->object == object )
			continue;
		FaceCorner *faceVert = face->corners;
		for ( int j = 0; j < 3; j++, faceVert++ ) {
			if ( VertsMatch( &model.verts[faceVert->index], vert, true ) ) {
				vert->merged = true;
				model.verts[faceVert->index].merged = true;
				return;
			}
		}
	}
}

/*
=============
SkelModel::LinkFaceVertex
=============
*/
void SkelModel::LinkFaceVertex( int faceIndex, int vertexIndex ) {
	FaceCorner *vert = &model.faces[faceIndex].corners[vertexIndex];
	if ( !model.verts[vert->index].merged )
		return;
	for ( int i = faceIndex + 1; i < model.numfaces; i++ ) {
		FaceCorner *other = model.faces[i].corners;
		for ( int j = 0; j < 3; j++, other++ ) {
			if ( other->index == vert->index ) {
				FaceCorner *link = other;
				do {
					if ( link == vert )
						return;
					link = link->next;
				} while ( link != other );
				FaceCorner *next = vert->next;
				FaceCorner *prev = other->prev;
				vert->next = other;
				other->prev = vert;
				prev->next = next;
				next->prev = prev;
				return;
			}
		}
	}
}

#line 2516
/*
=============
SkelModel::MergeDuplicateVerts
=============
*/
void SkelModel::MergeDuplicateVerts() {
	int i;

	model.numverts = 0;
	for ( i = 0; i < model.numSourceVerts; i++ ) {
		model.vertRemap[i] = FindVertex( &model.verts[i] );
		if ( model.vertRemap[i] != model.numverts ) {
			if ( model.verts[i].merged ) {
				model.verts[model.vertRemap[i]].merged = true;
			}
		} else {
			assert( model.numverts <= i );
			memcpy( &model.verts[model.numverts], &model.verts[i], sizeof( ModelVert ) );
			model.numverts++;
		}
	}
}

// True when every pair of normals on the ring agrees in direction.
/*
=============
NormalsAgree
=============
*/
static bool NormalsAgree( FaceCorner *head ) {
	FaceCorner *a, *b;

	a = head;
	do {
		for ( b = a->next; b != head; b = b->next ) {
			if ( b->normal[0] * a->normal[0] + b->normal[1] * a->normal[1] + b->normal[2] * a->normal[2] <= 0 ) {
				return false;
			}
		}
		a = a->next;
	} while ( a != head );
	return true;
}

/*
=============
SkelModel::SmoothNormals
=============
*/
void SkelModel::SmoothNormals() {
	int i, j, k;
	bool changed;
	int count;
	int v0, v1, v2;
	ModelFace *face, *other;
	FaceCorner *corner;

	for ( i = 0; i < model.numfaces; i++ ) {
		for ( k = 0; k < 3; k++ ) {
			MarkSplitVertex( i, k );
		}
	}
	memcpy( model.vertsSaved, model.verts, model.numSourceVerts * sizeof( ModelVert ) );
	changed = false;
	MergeDuplicateVerts();
	for ( face = model.faces, i = 0; i < model.numfaces; i++, face++ ) {
		for ( k = 0; k < 3; k++ ) {
			v0 = model.vertRemap[face->corners[k].index];
			v1 = model.vertRemap[face->corners[( k + 1 ) % 3].index];
			v2 = model.vertRemap[face->corners[( k + 2 ) % 3].index];
			if ( !model.verts[v0].merged && !model.verts[v1].merged ) {
				continue;
			}
			count = 0;
			for ( other = model.faces, j = 0; j < model.numfaces; j++, other++ ) {
				for ( int m = 0; m < 3; m++ ) {
					int w = model.vertRemap[other->corners[m].index];
					if ( ( v0 == w && v1 == model.vertRemap[other->corners[( m + 1 ) % 3].index] )
						|| ( v1 == w && v0 == model.vertRemap[other->corners[( m + 1 ) % 3].index] ) ) {
						count++;
						if ( i != j && v2 == model.vertRemap[other->corners[( m + 2 ) % 3].index] ) {
							model.verts[v0].merged = false;
							model.verts[v1].merged = false;
							changed = true;
							model.verts[v2].merged = false;
						}
					}
				}
			}
			if ( count > 2 ) {
				model.verts[v0].merged = false;
				changed = true;
				model.verts[v1].merged = false;
			}
		}
	}
	if ( changed ) {
		for ( i = 0; i < model.numSourceVerts; i++ ) {
			model.vertsSaved[i].merged = model.verts[model.vertRemap[i]].merged;
		}
		memcpy( model.verts, model.vertsSaved, model.numSourceVerts * sizeof( ModelVert ) );
		MergeDuplicateVerts();
	}
	for ( face = model.faces, i = 0; i < model.numfaces; i++, face++ ) {
		corner = face->corners;
		for ( k = 0; k < 3; k++, corner++ ) {
			corner->index = model.vertRemap[corner->index];
			corner->next = corner;
			corner->prev = corner;
		}
	}
	for ( i = 0; i < model.numfaces; i++ ) {
		for ( k = 0; k < 3; k++ ) {
			LinkFaceVertex( i, k );
		}
	}
	for ( face = model.faces, i = 0; i < model.numfaces; i++, face++ ) {
		for ( k = 0; k < 3; k++ ) {
			corner = &face->corners[k];
			if ( corner->next == corner ) {
				continue;
			}
			if ( NormalsAgree( corner ) ) {
				vec3_t sum;
				FaceCorner *c = corner;
				sum[0] = sum[1] = sum[2] = 0;
				do {
					sum[0] += c->normal[0];
					sum[1] += c->normal[1];
					sum[2] += c->normal[2];
					c = c->next;
				} while ( c != corner );
				VectorNormalize( sum );
				c = corner;
				do {
					FaceCorner *next = c->next;
					c->next = c;
					c->normal[0] = sum[0];
					c->normal[1] = sum[1];
					c->normal[2] = sum[2];
					c = next;
				} while ( c != corner );
			} else {
				FaceCorner *c = corner;
				do {
					FaceCorner *next = c->next;
					c->next = c;
					c = next;
				} while ( c != corner );
			}
		}
	}
}

#line 2710
/*
=============
SkelModel::CalculateBoneBounds
=============
*/
void SkelModel::CalculateBoneBounds() {
	int b, vertIndex;
	vec3_t mins, maxs;
	vec3_t delta, local;
	float inverse[3][3];
	BonePose *pose;
	ModelBone *bone;

	for ( b = 0; b < model.numbones; b++ ) {
		mins[0] = mins[1] = mins[2] = 0;
		maxs[0] = maxs[1] = maxs[2] = 0;
		for ( vertIndex = 0; vertIndex < model.numverts; vertIndex++ ) {
			assert( model.verts[vertIndex].numbones > 0 );
			if ( model.verts[vertIndex].blend[0].bone == b ) {
				pose = &model.poses->pose[model.verts[vertIndex].blend[0].bone];
				delta[0] = model.verts[vertIndex].pos[0] - pose->origin[0];
				delta[1] = model.verts[vertIndex].pos[1] - pose->origin[1];
				delta[2] = model.verts[vertIndex].pos[2] - pose->origin[2];
				MatrixTranspose( pose->axis, inverse );
				MatrixTransformVector( delta, inverse, local );
				if ( local[0] < mins[0] ) {
					mins[0] = local[0];
				}
				if ( local[0] > maxs[0] ) {
					maxs[0] = local[0];
				}
				if ( local[1] < mins[1] ) {
					mins[1] = local[1];
				}
				if ( local[1] > maxs[1] ) {
					maxs[1] = local[1];
				}
				if ( local[2] < mins[2] ) {
					mins[2] = local[2];
				}
				if ( local[2] > maxs[2] ) {
					maxs[2] = local[2];
				}
			}
		}
		bone = &model.bones[b];
		bone->mins[0] = mins[0];
		bone->mins[1] = mins[1];
		bone->mins[2] = mins[2];
		bone->maxs[0] = maxs[0];
		bone->maxs[1] = maxs[1];
		bone->maxs[2] = maxs[2];
	}
}

/*
=============
SkelModel::CopyBoneBounds
=============
*/
void SkelModel::CopyBoneBounds( SkelModel *source ) {
	int i, j;
	ModelBone *bone, *sourceBone;

	for ( i = 0; i < source->model.numbones; i++ ) {
		sourceBone = &source->model.bones[i];
		const char *name = sourceBone->name;
		for ( j = 0; j < model.numbones; j++ ) {
			bone = &model.bones[j];
			if ( !strcmp( name, bone->name ) ) {
				bone->mins[0] = sourceBone->mins[0];
				bone->mins[1] = sourceBone->mins[1];
				bone->mins[2] = sourceBone->mins[2];
				bone->maxs[0] = sourceBone->maxs[0];
				bone->maxs[1] = sourceBone->maxs[1];
				bone->maxs[2] = sourceBone->maxs[2];
				break;
			}
		}
	}
}

/*
=============
Animation::LoadModel
=============
*/
void Animation::LoadModel( const char *filename ) {
	Script script;
	script.LoadFile( filename );
	ReadModel( script );
}

/*
=============
Animation::ReadModel
=============
*/
void Animation::ReadModel( Script &script ) {
	script.MatchToken( "MODEL", true );
	script.MatchToken( "VERSION", true );
	int version = script.GetInteger( false );
	if ( version != 5 ) {
		script.Error( "Expecting version %d but found version %d.\n", 5, version );
	}
	LoadBones( script );
	LoadBasePose( script );
}

/*
=============
SkelModel::SkelModel
=============
*/
SkelModel::SkelModel() {
	Init();
}

/*
=============
SkelModel::~SkelModel
=============
*/
SkelModel::~SkelModel() {
	Free();
}

/*
=============
SkelModel::UsesRadialNormals
=============
*/
bool SkelModel::UsesRadialNormals( const char *material ) {
	char *text;
	char *token;

	text = shaderLibrary.FindShader( material );
	if ( !text ) {
		return false;
	}
	token = Com_Parse( &text );
	Com_SkipRestOfLine( &text );
	if ( strcmp( token, "{" ) ) {
		printf( "*** WARNING: Missing '{' at start of shader [%s] ***\n", material );
		return false;
	}
	while ( ( token = Com_Parse( &text ) ) != NULL && token[0] ) {
		if ( !stricmp( token, "}" ) ) {
			return false;
		}
		if ( !stricmp( token, "radialNormals" ) ) {
			return true;
		}
		if ( !stricmp( token, "{" ) ) {
			Com_UngetToken();
			Com_SkipBracedSection( &text, 0x7fffffff );
		} else {
			Com_SkipRestOfLine( &text );
		}
	}
	return false;
}

/*
=============
SkelModel::ReadMaterials
=============
*/
void SkelModel::ReadMaterials( Script *script, int numExtra ) {
	int count, size, i;
	MaterialName *material;

	script->MatchToken( "NUMMATERIALS", true );
	count = script->GetInteger( false );
	size = ( numExtra + count * 2 ) * sizeof( MaterialName );
	model.materialNames = new MaterialName[numExtra + count * 2];
	memset( model.materialNames, 0, size );
	model.numMaterials = 0;
	for ( i = 0; i < count; i++ ) {
		script->MatchToken( "MATERIAL", true );
		material = &model.materialNames[model.numMaterials++];
		script->GetInteger( false );
		memset( material->name, 0, sizeof( material->name ) );
		strncpy( material->name, script->GetToken( false ), sizeof( material->name ) - 1 );
		if ( radialNormals ) {
			material->radialNormals = UsesRadialNormals( material->name );
		}
	}
}

/*
=============
SkelModel::ReadVerts
=============
*/
void SkelModel::ReadVerts( Script *script ) {
	int i, j;
	ModelVert *vert;
	Blend *blend;

	script->MatchToken( "NUMVERTS", true );
	model.numSourceVerts = script->GetInteger( false );
	model.verts = new ModelVert[model.numSourceVerts];
	model.vertsSaved = new ModelVert[model.numSourceVerts];
	memset( model.verts, 0, model.numSourceVerts * sizeof( ModelVert ) );
	model.vertRemap = new int[model.numSourceVerts];
	for ( i = 0; i < model.numSourceVerts; i++ ) {
		vert = &model.verts[i];
		script->MatchToken( "VERT", true );
		if ( script->GetInteger( false ) != i ) {
			script->Error( "Vertex number out of sync" );
		}
		script->MatchToken( "OFFSET", true );
		script->GetVector( vert->pos, false );
		script->MatchToken( "BONES", true );
		vert->numbones = script->GetInteger( false );
		if ( vert->numbones >= MAX_VERT_BLENDS ) {
			Error( "Exceeded %d bones on vertex %d\n", MAX_VERT_BLENDS, i );
		}
		for ( blend = vert->blend, j = 0; j < vert->numbones; j++, blend++ ) {
			script->MatchToken( "BONE", true );
			blend->bone = script->GetInteger( false );
			if ( blend->bone < 0 || blend->bone >= model.numRootBones ) {
				script->Error( "Vertex %d has bad bone\n", j, blend->bone );
			}
			blend->bone = model.boneRemap[blend->bone];
			blend->weight = script->GetFloat( false );
			if ( blend->weight < 0 ) {
				script->Error( "Vertex %d has negative weight %.2f\n", j, blend->weight );
			}
		}
		NormalizeBlends( vert, i );
	}
}

#line 1284
/*
=============
SkelModel::BuildSurface
=============
*/
void SkelModel::BuildSurface( int object, int mtlID ) {
	ModelSurface surf;
	ModelSurface *current;
	ModelFace *face;
	int i, j, firstSurface, bone;

	assert( mtlID >= 0 );
	firstSurface = model.numsurfaces;
	memset( &surf, 0, sizeof( surf ) );
	for ( i = 0, face = model.faces; i < model.numfaces; i++, face++ ) {
		if ( face->object == object && face->material == mtlID ) {
#line 1307
			assert( !face->bUsed );
			face->bUsed = true;
			face->loadIndex = AddSurfaceTri( &surf, face->corners, face->object, mtlID );
			if ( face->loadIndex >= 0 ) {
				model.unusedFaces--;
			} else {
				printf( "*** WARNING: culling face from material '%s'\n", model.materialNames[mtlID].name );
				face->material = -1;
			}
		}
	}
	while ( surf.ResolveVertexBones() ) {
	}
	for ( i = 0, face = model.faces; i < model.numfaces; i++, face++ ) {
		if ( face->object == object && face->material == mtlID ) {
#line 1333
			assert( face->loadIndex >= 0 );
			bone = surf.verts[surf.tris[face->loadIndex].index[0]].bone;
			for ( j = firstSurface; j < model.numsurfaces; j++ ) {
				current = &model.surfaces[j];
				if ( current->boneIndex == bone ) {
					break;
				}
			}
			if ( j == model.numsurfaces ) {
				current = AddSurface( object, mtlID, bone );
			}
			AddSurfaceTri( current, face->corners, face->object, mtlID );
		}
	}
}

/*
=============
SkelModel::BuildSurfaces
=============
*/
void SkelModel::BuildSurfaces() {
	int i, j;

	model.maxsurfaces = model.numObjects * model.numMaterials;
	model.surfaces = new ModelSurface[model.maxsurfaces];
	memset( model.surfaces, 0, model.maxsurfaces * sizeof( ModelSurface ) );
	model.unusedFaces = model.numfaces;
	model.numsurfaces = 0;
	for ( i = 0; i < model.numMaterials; i++ ) {
		for ( j = 0; j < model.numObjects; j++ ) {
			BuildSurface( j, i );
		}
	}
	delete[] model.faces;
	model.faces = NULL;
}

/*
=============
SkelModel::ReadModel
=============
*/
void SkelModel::ReadModel( Script *script ) {
	int version;

	script->MatchToken( "MODEL", true );
	script->MatchToken( "VERSION", true );
	version = script->GetInteger( false );
	if ( version != 5 && version != 4 ) {
		script->Error( "Expecting version %d or %d but found version %d.\n", 4, 5, version );
	}
	LoadBones( script );
	if ( model.numbones <= 0 ) {
		Error( "model has no bones" );
	}
	ReadBasePose( script );
	ReadVerts( script );
	ReadFaces( script );
}

/*
=============
SkelModel::ReadModelAndMaterials
=============
*/
void SkelModel::ReadModelAndMaterials( Script *script, int numExtra ) {
	ReadModel( script );
	LoadObjects( script );
	ReadMaterials( script, numExtra );
}

/*
=============
SkelModel::LoadModel
=============
*/
void SkelModel::LoadModel( const char *filename ) {
	Script script;

	script.LoadFile( filename );
	ReadModel( &script );
}

/*
=============
SkelModel::LoadModelAndMaterials
=============
*/
void SkelModel::LoadModelAndMaterials( const char *filename, int numExtra ) {
	Script script;

	script.LoadFile( filename );
	ReadModelAndMaterials( &script, numExtra );
}

/*
=============
BuildModel
=============
*/
void BuildModel( ModelLods *lods, int lod, const char *source, const char *parts,
	const char *sourceDir, const char *gameDir, const char *skinDir,
	bool writeOffsets, const char *surfs, const char *controllers, const char *stabilizers,
	BoneNames *bones, bool radialNormals, const float *radialOrigin, int animated,
	const char *rigidRoot, SurfaceNames *surfNames, SkinOverride *overrides, int numOverrides,
	bool viewmodel, const char *hitBox ) {
	SkelModel *model, *hitModel;
	int i;

	model = new SkelModel;
	model->sourceDir = sourceDir;
	model->targetRoot = gameDir;
	model->skinDir = skinDir;
	model->writeOffsets = writeOffsets;
	model->surfsFile = surfs;
	model->LoadBoneControllers( controllers );
	model->LoadBoneStabilizers( stabilizers );
	model->viewmodel = viewmodel;
	model->radialNormals = radialNormals;
	model->radialOrigin[0] = radialOrigin[0];
	model->radialOrigin[1] = radialOrigin[1];
	model->radialOrigin[2] = radialOrigin[2];
	model->LoadModelAndMaterials( source, numOverrides );
	model->CalculateEdgeLengths();
	model->SmoothNormals();
	if ( !animated ) {
		model->MakeRigid( rigidRoot );
	} else if ( rigidRoot[0] ) {
		Error( "rigid root '%s' specified without lod being forced rigid", rigidRoot );
	}
	model->CheckVertexRoots();
	model->CalculateBoneBounds();
	model->BuildSurfaces();
	model->TransformNormalsToBones();
	model->CheckSurfaceTexCoords();
	if ( parts ) {
		model->ClassifyBones();
		if ( hitBox[0] ) {
			hitModel = new SkelModel;
			hitModel->LoadBoneControllers( controllers );
			hitModel->LoadBoneStabilizers( stabilizers );
			hitModel->LoadModel( hitBox );
			hitModel->CalculateEdgeLengths();
			hitModel->SmoothNormals();
			if ( !animated ) {
				hitModel->MakeRigid( rigidRoot );
			}
			hitModel->CheckVertexRoots();
			hitModel->CalculateBoneBounds();
			model->CopyBoneBounds( hitModel );
			delete hitModel;
		}
		model->WriteParts( parts );
	}
	model->ApplySkinOverrides( surfNames, overrides, numOverrides );
	model->WriteSurfaces( surfs );
	if ( lods->mins[0] > lods->maxs[0] ) {
		model->CalculateBounds( lods );
	}
	if ( lods->collisionLod == lod ) {
		model->BuildCollisionModel( collmodel );
	}
	for ( i = 0; i < model->model.numbones; i++ ) {
		bones->parents[i] = model->model.bones[i].parent;
		strcpy( bones->names[i], model->model.bones[i].name );
	}
	bones->count = model->model.numbones;
	delete model;
}

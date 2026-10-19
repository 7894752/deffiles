/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Model and animation export loading, hierarchy building and surface setup.

#include "../xmodel/skelmodel.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <malloc.h>
#include <ctype.h>
#include "xanim_model.h"
#include <math.h>
#include <float.h>
#include "common/q_round.h"


extern "C" {
void AddPointToBounds( const float *v, float *mins, float *maxs );
float VectorDistance( const float *p1, const float *p2 );
float VectorDistanceSquared( const float *p1, const float *p2 );
float VectorNormalize( float *v );
int VectorCompareEpsilon( const float *, const float * );
}
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
Animation::LoadBoneStabilizers
=============
*/
void Animation::LoadBoneStabilizers( const char *text ) {
	model.numStabilizers = 0;
	while ( ( text = COM_Parse( text ) ) != 0 ) {
		if ( model.numStabilizers >= MAX_BONE_STABILIZERS ) {
			Error( "maximum bone stabilizers exceeded" );
		}
		strcpy( model.stabilizerBone[model.numStabilizers], com_token );
		strlower( model.stabilizerBone[model.numStabilizers] );
		text = COM_Parse( text );
		if ( !text ) {
			Error( "no bone stabilizer name specified for %s", model.stabilizerBone[model.numStabilizers] );
		}
		strcpy( model.stabilizerName[model.numStabilizers], com_token );
		strlower( model.stabilizerName[model.numStabilizers] );
		model.numStabilizers++;
	}
}

// Model surfaces and vertex processing.
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

/*
=============
Animation::LoadParts

Map animated parts to model bones and remove unused bones.
=============
*/
void Animation::LoadParts( Script &script, const char *nodeName ) {
	int i, j, partNumber, bonenum;
	char *partName;
	Bone *bone;

	// Parts are listed independently of the model skeleton. A part which is
	// absent from the model can still be selected as the motion node.

	script.MatchToken( "NUMPARTS", true );
	model.numNodeParts = script.GetInteger( false );
	model.numParts = model.numStabilizers + model.numNodeParts;
	for ( i = 0; i < model.numbones; i++ ) {
		model.bones[i].used = false;
	}
	model.animToModel = new short[model.numParts];
	model.nodePart = -1;

	// Resolve the exported part names before adding stabilizer parts.

	for ( i = 0; i < model.numNodeParts; i++ ) {
		script.MatchToken( "PART", true );
		partNumber = script.GetInteger( false );
		if ( partNumber != i ) {
			script.Error( "Part number out of sync" );
		}
		model.animToModel[partNumber] = -1;
		partName = script.GetString( false );
		bone = model.bones;
		for ( j = 0; j < model.numbones; j++, bone++ ) {
			if ( !stricmp( partName, bone->name ) ) {
				if ( bone->used ) {
					script.Error( "Duplicate part \"%s\" in animation export.", partName );
				}
				bone->used = true;
				model.animToModel[partNumber] = j;
				break;
			}
		}

		// The motion node must remain separate from the model hierarchy.
		if ( nodeName[0] ) {
			if ( !stricmp( partName, nodeName ) ) {
				if ( j != model.numbones ) {
					script.Error( "Node part \"%s\" cannot be in the model.", nodeName );
				}
				if ( model.nodePart != -1 ) {
					script.Error( "Duplicate node part \"%s\" in animation export.", partName );
				}
				model.nodePart = partNumber;
			}
		}
	}

	// Stabilizers use their generated bone names, rather than exported parts.
	// They are appended to the part-to-bone map in stabilizer order.

	for ( bonenum = 0; i < model.numParts && bonenum < model.numStabilizers; i++, bonenum++ ) {
		model.animToModel[i] = -1;
		partName = model.stabilizerName[bonenum];
		bone = model.bones;
		for ( j = 0; j < model.numbones; j++, bone++ ) {
			if ( !stricmp( partName, bone->name ) ) {
				if ( bone->used ) {
					script.Error( "Duplicate bone stabilizer part \"%s\" in animation export.", partName );
				}
				bone->used = true;
				model.animToModel[i] = j;
				break;
			}
		}
	}

	if ( nodeName[0] && model.nodePart == -1 ) {
		Error( "Node part \"%s\" not found.", nodeName );
	}

	// Reparent animated bones to their nearest animated ancestor before
	// compacting the table; intermediate unused bones will disappear.

	for ( i = 0; i < model.numbones; i++ ) {
		bone = &model.bones[i];
		while ( bone->parent != -1 && !model.bones[bone->parent].used ) {
			bone->parent = model.bones[bone->parent].parent;
		}
	}

	// Shift the remaining bones and repair both parent and part indices
	// whenever an unused bone is removed.

	for ( bonenum = 0; bonenum < model.numbones; bonenum++ ) {
		if ( !model.bones[bonenum].used ) {
			for ( j = bonenum + 1; j < model.numbones; j++ ) {

				bone = &model.bones[j];

				assert( bone->parent != bonenum );
				if ( bone->parent > bonenum ) {
					bone->parent--;
				}
				model.bones[j - 1] = *bone;
			}

			for ( i = 0; i < model.numParts; i++ ) {
				assert( model.animToModel[i] != bonenum );
				if ( model.animToModel[i] > bonenum ) {
					model.animToModel[i]--;
				}
			}
			model.numbones--;
		}
	}
}

/*
=============
Animation::LoadFrames
=============
*/
void Animation::LoadFrames( Script &script ) {
	int i, j, number, framerate, numframes, boneIndex, nodeBone;
	BoneFrame *bone, *parentBone, dummy;
	vec3_t offset, scale, axis[3];

	nodeBone = -1;
	if ( model.numStabilizers ) {
		for ( i = 0; i < model.numbones; i++ ) {
			if ( !stricmp( model.bones[i].name, "tag_origin" ) ) {
				nodeBone = i;
				break;
			}
		}
		if ( nodeBone < 0 ) {
			script.Error( "Model does not have a tag_origin bone" );
		}
	}

	script.MatchToken( "FRAMERATE", true );
	framerate = script.GetInteger( false );
	if ( framerate <= 0 ) {
		script.Error( "Framerate must be positive (using %d).", framerate );
	}
	script.MatchToken( "NUMFRAMES", true );
	numframes = script.GetInteger( false );
	if ( numframes <= 0 ) {
		script.Error( "numframes must be positive" );
	}
	model.numframes = numframes;
	model.framerate = framerate;
	printf( "framerate: %d\n", framerate );

	Frame *&frames = model.frames;
	if ( frames ) {
		delete [] frames;
	}
	frames = new Frame[numframes];
	model.nodeFrames = new BoneFrame[numframes];

	for ( i = 0; i < numframes; i++ ) {
		frames[i].bones = new BoneFrame[model.numbones];
		script.MatchToken( "FRAME", true );
		frames[i].number = script.GetInteger( false );
		for ( j = 0; j < model.numNodeParts; j++ ) {
			script.MatchToken( "PART", true );
			number = script.GetInteger( false );
			if ( number != j ) {
				script.Error( "Bone number out of sync" );
			}
			script.MatchToken( "OFFSET", true );
			script.GetVector( offset, false );
			script.MatchToken( "SCALE", true );
			script.GetVector( scale, false );
			script.MatchToken( "X", true );
			script.GetVector( axis[0], false );
			script.MatchToken( "Y", true );
			script.GetVector( axis[1], false );
			script.MatchToken( "Z", true );
			script.GetVector( axis[2], false );

			boneIndex = model.animToModel[j];
			if ( boneIndex >= 0 ) {
				bone = &frames[i].bones[boneIndex];
			} else if ( j == model.nodePart ) {
				bone = &model.nodeFrames[i];
			} else {
				bone = &dummy;
			}
			VectorCopy( offset, bone->offset );
			VectorCopy( scale, bone->scale );
			VectorCopy( axis[0], bone->axis[0] );
			VectorCopy( axis[1], bone->axis[1] );
			VectorCopy( axis[2], bone->axis[2] );
		}
		for ( ; j < model.numParts; j++ ) {
			bone = &frames[i].bones[model.animToModel[j]];
			boneIndex = model.bones[model.animToModel[j]].parent;
			if ( boneIndex < 0 ) {
				script.Error( "Animation does not have a tag_origin bone in it" );
			}
			parentBone = &frames[i].bones[boneIndex];
			VectorCopy( parentBone->offset, bone->offset );
			VectorCopy( parentBone->scale, bone->scale );
			parentBone = &frames[i].bones[nodeBone];
			VectorCopy( parentBone->axis[0], bone->axis[0] );
			VectorCopy( parentBone->axis[1], bone->axis[1] );
			VectorCopy( parentBone->axis[2], bone->axis[2] );
		}
	}
}

/*
=============
Animation::AddNotetrack
=============
*/
void Animation::AddNotetrack( const char *name, int frame ) {
	int i, j, k;
	NotetrackChannel *channel;

	for ( i = 0; i < model.numChannels; i++ ) {
		channel = &model.channels[i];
		if ( !strcmp( name, channel->name ) ) {
			goto found;
		}
	}
	if ( model.numChannels >= MAX_NOTETRACK_CHANNELS ) {
		Error( "MAX_NOTETRACK_CHANNELS exceeded" );
	}
	channel = &model.channels[model.numChannels];
	if ( strlen( name ) >= NOTETRACK_NAME_LEN ) {
		Error( "NOTETRACK_NAME_LEN exceeded" );
	}
	if ( !strcmp( name, "end" ) ) {
		Error( "'end' is not an allowed user specified notetrack" );
	}
	strcpy( channel->name, name );
	channel->numFrames = 0;
	model.numChannels++;

found:
	for ( j = 0; j < channel->numFrames; j++ ) {
		if ( frame <= channel->frames[j] ) {
			if ( frame == channel->frames[j] ) {
				return;
			}
			break;
		}
	}
	if ( channel->numFrames >= MAX_NOTETRACK_FRAMES ) {
		Error( "MAX_NOTETRACK_FRAMES exceeded" );
	}
	for ( k = channel->numFrames - 1; k >= j; k-- ) {
		channel->frames[k + 1] = channel->frames[k];
	}
	channel->frames[j] = frame;
	channel->numFrames++;
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

/*
=============
TrimWhitespace
=============
*/
static char *TrimWhitespace( char *s ) {
	int len;

	while ( iswspace( *s ) ) {
		s++;
	}
	for ( len = strlen( s ) - 1; len >= 0 && iswspace( s[len] ); len-- ) {
		s[len] = 0;
	}
	return s;
}

/*
=============
Animation::LoadNotetracks
=============
*/
void Animation::LoadNotetracks( Script &script ) {
	model.numChannels = 0;
	script.MatchToken( "NOTETRACKS", true );
	for ( int i = 0; i < model.numNodeParts; i++ ) {
		script.MatchToken( "PART", true );
		if ( script.GetInteger( false ) != i ) {
			script.Error( "Bone number out of sync" );
		}
		script.MatchToken( "NUMTRACKS", true );
		int numTracks = script.GetInteger( false );
		for ( int j = 0; j < numTracks; j++ ) {
			script.MatchToken( "NOTETRACK", true );
			if ( script.GetInteger( false ) != j ) {
				script.Error( "Notetrack number out of sync" );
			}
			script.MatchToken( "NUMKEYS", true );
			int numKeys = script.GetInteger( false );
			for ( int k = 0; k < numKeys; k++ ) {
				script.MatchToken( "FRAME", true );
				int number = script.GetInteger( false );
				char *name = script.GetToken( false );
				int frame = FindFrame( number );
				if ( frame >= 0 ) {
					int len = strlen( name ) + 1;
					char *lowerName = (char *)_alloca( len );
					for ( int n = len - 1; n >= 0; n-- ) {
						lowerName[n] = tolower( name[n] );
					}
					char *trimmed = TrimWhitespace( lowerName );
					AddNotetrack( trimmed, frame );
				}
			}
		}
	}
}

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
#line 1066 "buildmds.cpp"
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

/*
=============
Animation::CopyBasePose
=============
*/
void Animation::CopyBasePose( const Animation &source ) {
	model.numframes = 1;
	model.numbones = source.model.numbones;
	model.frames = new Frame[1];
	model.frames[0].bones = new BoneFrame[model.numbones];
	for ( int i = 0; i < model.numbones; i++ ) {
		BoneFrame *dest = &model.frames[0].bones[i];
		const BoneFrame *src = &source.model.frames[0].bones[i];
		VectorCopy( src->offset, dest->offset );
		VectorCopy( src->scale, dest->scale );
		AxisCopy( src->axis, dest->axis );
	}
}

extern "C" void Com_Error( int code, const char *format, ... );
extern "C" void MatrixTranspose( const float in[3][3], float out[3][3] );
extern "C" void MatrixMultiply( const float a[3][3], const float b[3][3], float out[3][3] );
void MatrixToQuat( float axis[3][3], float quat[4] );

#define QUAT_BONE_ENTRY_SIZE 4096
#define QUAT_BONE_LIST_SIZE 256

QuatBoneList g_quatBoneList[QUAT_BONE_LIST_SIZE];
QuatBoneEntry g_quatBoneEntries[QUAT_BONE_ENTRY_SIZE];
int g_quatBoneListSize = 0;
int g_quatBoneEntrySize = 0;
int g_quatBoneGroupStart = 0;

/*
=============
NormalizeFilename
=============
*/
static void NormalizeFilename( char *out, const char *filename, int count ) {
#line 2142 "buildmds.cpp"
	assert( count );
	char c;
	do {
		do { c = *filename++; } while ( c == '\\' || c == '/' );
		while ( (unsigned char)c >= ' ' ) {
			*out++ = tolower( c );
			if ( !--count ) {
				Com_Error( 1, "Filename '%s' exceeds maximum length of %d", filename, count );
			}
			if ( c == '\\' ) break;
			c = *filename++;
			if ( c == '/' ) c = '\\';
		}
	} while ( c );
	*out = 0;
}

/*
=============
Animation::Animation
=============
*/
Animation::Animation() {
	Init();
}

/*
=============
Animation::~Animation
=============
*/
Animation::~Animation() {
	Free();
}

/*
=============
Animation::LoadModel
=============
*/
void Animation::LoadModel( const char *filename ) {
	Script script;
	script.LoadFile( filename );
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
Animation::LoadAnimation
=============
*/
void Animation::LoadAnimation( const char *filename, const char *node ) {
	Script script;
	memset( model.name, 0, sizeof( model.name ) );
	strncpy( model.name, filename, sizeof( model.name ) - 1 );
	script.LoadFile( filename );
	script.MatchToken( "ANIMATION", true );
	script.MatchToken( "VERSION", true );
	int version = script.GetInteger( false );
	if ( version != 3 ) {
		script.Error( "Expecting version %d but found version %d.\n", 3, version );
	}
	LoadParts( script, node );
	LoadFrames( script );
	LoadNotetracks( script );
}

/*
=============
ConvertAnimation
=============
*/
void ConvertAnimation( AnimationOptions *options ) {
	Animation *animation = 0;
	Animation *basePose = 0;
	Animation *matchModel = 0;
	char filename[1024];
	try {
		animation = new Animation;
		basePose = new Animation;
		animation->model.looping = options->looping;
		animation->model.useBones = options->useBones;
		animation->model.rotationThreshold = options->rotationThreshold;
		animation->model.translationThreshold = options->translationThreshold;
		animation->LoadBoneStabilizers( options->stabilizers );
		animation->LoadModel( options->modelFilename );
		basePose->CopyBasePose( *animation );
		animation->LoadAnimation( options->animFilename, options->node );
		QuatBoneList *match = 0;
		NormalizeFilename( filename, options->modelFilename, sizeof( filename ) );
		for ( int i = 0; i < g_quatBoneListSize; i++ ) {
			if ( !strcmp( g_quatBoneList[i].filename.c_str(), filename ) ) {
				match = &g_quatBoneList[i];
				break;
			}
		}
		animation->WriteAnimation( options->outputFilename, options->type,
			basePose->model.frames[0].bones, match ? match->bones : 0 );
		delete animation;
		delete basePose;
		delete matchModel;
	} catch ( char *message ) {
		delete animation;
		delete basePose;
		delete matchModel;
		throw message;
	}
}

/*
=============
LoadQuatBones
=============
*/
void LoadQuatBones( const char *filename, bool first ) {
	char normalized[1024];
	float inverse[4][3];
	float axis[3][3];
	Animation *model;
	if ( first ) g_quatBoneGroupStart = g_quatBoneEntrySize;
#line 2208 "buildmds.cpp"
	assert( g_quatBoneListSize < QUAT_BONE_LIST_SIZE );

	NormalizeFilename( normalized, filename, sizeof( normalized ) );
	int i;
	for ( i = 0; i < g_quatBoneListSize; i++ ) {
		if ( !strcmp( g_quatBoneList[i].filename.c_str(), normalized ) ) {
			Error( "duplicate filename '%s'", filename );
		}
	}

	// Each model in a match group shares the same bone rotation table.
	g_quatBoneList[i].filename = normalized;
	g_quatBoneList[i].bones = &g_quatBoneEntries[g_quatBoneGroupStart];
	g_quatBoneListSize++;

	model = 0;

	try {
		model = new Animation;
		model->LoadModel( normalized );
		BoneFrame *basePose = model->model.frames[0].bones;
		for ( int j = 0; j < model->model.numbones; j++ ) {
			Bone *bone = &model->model.bones[j];
			QuatBoneEntry *entry;

			// The first model to name a bone supplies its reference rotation.
			for ( entry = &g_quatBoneEntries[g_quatBoneGroupStart]; entry != &g_quatBoneEntries[g_quatBoneEntrySize]; entry++ ) {
				if ( !stricmp( bone->name, entry->name.c_str() ) ) goto nextBone;
			}

			// Append new names once, preserving the order of the model skeleton.
			assert( g_quatBoneEntrySize < QUAT_BONE_ENTRY_SIZE );
			if ( g_quatBoneGroupStart != g_quatBoneEntrySize ) {
				g_quatBoneEntries[g_quatBoneEntrySize - 1].more = true;
			}
			int parentIndex = bone->fileIndex;
			BoneFrame *pose = &basePose[parentIndex];
			if ( bone->parent >= 0 ) parentIndex = model->model.bones[bone->parent].fileIndex;
			BoneFrame *parentPose = &basePose[parentIndex];
			MatrixTranspose( parentPose->axis, inverse );
			MatrixMultiply( pose->axis, inverse, axis );
			MatrixToQuat( axis, entry->quat );
			entry->name = bone->name;
			entry->more = false;
			g_quatBoneEntrySize++;
		nextBone:;
		}
		delete model;
	} catch ( char *message ) {
		delete model;
		throw message;
	}
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

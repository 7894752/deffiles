/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Bone hierarchy, animation frames and conversion options.

#ifndef ANIMATIONDATA_H
#define ANIMATIONDATA_H

#include <stdio.h>
#include "script.h"

#define MAX_BONES				256
#define MAX_NOTETRACK_CHANNELS	256
#define MAX_NOTETRACK_FRAMES	32
#define MAX_BONE_CONTROLLERS	8
#define MAX_BONE_STABILIZERS	2
#define NOTETRACK_NAME_LEN		128
#define MODEL_NAME_LEN			64

typedef float vec3_t[3];

#define VectorCopy( a, b )	( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define AxisCopy(a,b) ( VectorCopy((a)[0],(b)[0]), VectorCopy((a)[1],(b)[1]), VectorCopy((a)[2],(b)[2]) )
#define VectorClear( a ) ( ( a )[0] = ( a )[1] = ( a )[2] = 0 )

struct Bone {
	int               fileIndex;                           // index of this bone in the export file
	int               parent;
	int               reserved;
	char              name[MODEL_NAME_LEN];
	bool              used;                                // referenced by a part
	bool              set;
	int               classification;
	int               stabilizer;                          // created for a bone stabilizer
};

struct BoneFrame {
	vec3_t            offset;
	vec3_t            scale;
	vec3_t            axis[3];
	int               set;
};

struct Frame {
	int               number;
	BoneFrame         *bones;
};

struct NotetrackChannel {
	char              name[NOTETRACK_NAME_LEN];
	int               frames[MAX_NOTETRACK_FRAMES];
	int               numFrames;
};

struct ModelVert;
struct ModelFace;
struct ModelSurface;
struct MaterialName;

struct AnimationData {
	char              name[MODEL_NAME_LEN];
	int               framerate;
	int               numverts;
	int               numSourceVerts;
	int               numfaces;
	int               numParts;                            // node parts plus bone stabilizer parts
	int               numNodeParts;
	int               numbones;                            // including controllers and stabilizers
	int               numFileBones;
	int               numframes;
	int               numsurfaces;
	int               maxsurfaces;
	int               numObjects;
	int               numMaterials;
	int               unusedFaces;
	int               *vertRemap;
	ModelVert         *verts;
	ModelVert         *vertsSaved;
	ModelFace         *faces;
	Frame             *frames;
	Bone              *bones;
	ModelSurface      *surfaces;
	char              (*objectNames)[MODEL_NAME_LEN];
	MaterialName      *materialNames;
	int               nodePart;
	BoneFrame         *nodeFrames;
	short             *animToModel;                        // part index -> model bone
	int               *fileToModel;                        // export file bone -> model bone
	FILE              *file;
	const char        *sourceDir;
	const char        *targetRoot;
	const char        *skinDir;
	const char        *surfsFile;
	vec3_t            radialOrigin;
	bool              radialNormals;
	bool              looping;
	bool              useBones;
	float             rotationThreshold;
	float             translationThreshold;
	NotetrackChannel  channels[MAX_NOTETRACK_CHANNELS];
	int               numChannels;
	int               numControllers;
	char              controllerBone[MAX_BONE_CONTROLLERS][MODEL_NAME_LEN];
	char              controllerName[MAX_BONE_CONTROLLERS][MODEL_NAME_LEN];
	int               controllerUsed[MAX_BONE_CONTROLLERS];
	int               numStabilizers;
	char              stabilizerBone[MAX_BONE_STABILIZERS][MODEL_NAME_LEN];
	char              stabilizerName[MAX_BONE_STABILIZERS][MODEL_NAME_LEN];
	int               stabilizerUsed[MAX_BONE_STABILIZERS];
};

struct BoneRotation {
	std::string       name;
	float             quat[4];
	bool              more;
};

class Animation {
public:
	/*
	=============
	Animation::Animation
	=============
	*/
	Animation() { Init(); }

	/*
	=============
	Animation::~Animation
	=============
	*/
	~Animation() { Free(); }

	void	Init();
	void	Free();
	void	LoadBoneStabilizers( const char *text );
	void	LoadBones( Script &script );
	void	LoadParts( Script &script, const char *nodeName );
	void	LoadFrames( Script &script );
	void	AddNotetrack( const char *name, int frame );
	int		FindFrame( int number );
	void	LoadNotetracks( Script &script );
	void	LoadBasePose( Script &script );
	void	CopyBasePose( const Animation &src );
	void	ReadModel( Script &script );
	void	LoadModel( const char *filename );
	void	LoadAnimation( const char *filename, const char *nodeName );

	// Animation export writer.
	void	WriteVec3( float *v );
	void	WriteQuatXYZ( short *iQuat );
	void	WriteQuatZ( short *iQuat );
	void	WriteNotetracks();
	bool	WriteQuatFrames( float (*_quatFrames)[4], bool bKeepIdentity );
	void	WriteTransFrames( float (*trans)[3] );
	void	WriteQuatFrames2D( float (*_quatFrames)[2] );
	void	WriteAnimation( const char *filename, int type, BoneFrame *baseFrames, const BoneRotation *rotations );

	AnimationData	model;
};

extern "C" char com_token[];
extern "C" const char *COM_Parse( const char *text );
extern "C" char *strlower( char *s );
extern "C" void Error( const char *fmt, ... );
void		*Z_Malloc( int size );
void		Z_Free( void *ptr );


#endif

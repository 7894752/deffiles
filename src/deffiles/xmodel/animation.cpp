/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// xmodel converter: model/surface/skin output.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <malloc.h>
#include <limits.h>
#include "../convert_value.h"
#include "skelmodel.h"

extern "C" float VectorNormalize4D( float q[4] );
extern CollModel collmodel;
void MatrixToQuat( float m[3][3], float quat[4] );

/*
=============
SkelModel::WriteVec3
=============
*/
void SkelModel::WriteVec3( const vec3_t v ) {
	vec3_t out;

	out[0] = v[0];
	out[1] = v[1];
	out[2] = v[2];
	SafeWrite( file, out, sizeof( out ) );
}

/*
=============
MatrixToQuat
=============
*/
void MatrixToQuat( float m[3][3], float quat[4] ) {
	float q[4][4], mag[4];
	int best;

	q[0][0] = m[1][2] - m[2][1];
	q[0][1] = m[2][0] - m[0][2];
	q[0][2] = m[0][1] - m[1][0];
	q[0][3] = m[0][0] + m[1][1] + m[2][2] + 1.0f;
	mag[0] = fabs( q[0][0] ) + fabs( q[0][1] ) + fabs( q[0][2] ) + fabs( q[0][3] );

	q[1][0] = m[0][0] - m[1][1] - m[2][2] + 1.0f;
	q[1][1] = m[0][1] + m[1][0];
	q[1][2] = m[0][2] + m[2][0];
	q[1][3] = m[1][2] - m[2][1];
	mag[1] = fabs( q[1][0] ) + fabs( q[1][1] ) + fabs( q[1][2] ) + fabs( q[1][3] );

	q[2][0] = m[0][1] + m[1][0];
	q[2][1] = m[1][1] - m[0][0] - m[2][2] + 1.0f;
	q[2][2] = m[1][2] + m[2][1];
	q[2][3] = m[2][0] - m[0][2];
	mag[2] = fabs( q[2][0] ) + fabs( q[2][1] ) + fabs( q[2][2] ) + fabs( q[2][3] );

	q[3][0] = m[0][2] + m[2][0];
	q[3][1] = m[1][2] + m[2][1];
	q[3][2] = m[2][2] - m[1][1] - m[0][0] + 1.0f;
	q[3][3] = m[0][1] - m[1][0];
	mag[3] = fabs( q[3][0] ) + fabs( q[3][1] ) + fabs( q[3][2] ) + fabs( q[3][3] );

	best = 0;
	if ( mag[1] > mag[best] ) {
		best = 1;
	}
	if ( mag[2] > mag[best] ) {
		best = 2;
	}
	if ( mag[3] > mag[best] ) {
		best = 3;
	}

	memcpy( quat, q[best], sizeof( q[best] ) );
	if ( quat[3] < 0 ) {
		for ( int i = 0; i < 4; i++ ) {
			quat[i] = -quat[i];
		}
	}
	VectorNormalize4D( quat );
}

// Quantize xyz, then reconstruct w on the integer quaternion sphere.
#line 67
/*
=============
QuatToShort
=============
*/
static void QuatToShort( float *quat, short *iQuat ) {
	float q[4];
	float S;
	int iQ[4];
	int w;
	int k;

	q[0] = quat[0];
	q[1] = quat[1];
	q[2] = quat[2];
	q[3] = quat[3];
	if ( q[3] < 0 ) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}

	S = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	assert( S );
	S = SHRT_MAX / sqrt( S );

	for ( k = 0; k < 3; k++ ) {
		iQ[k] = (int)floor( S * q[k] + 0.5f );

		assert( iQ[k] == (short) iQ[k] );
		iQuat[k] = (short) iQ[k];
	}

	w = SHRT_MAX * SHRT_MAX - iQ[0] * iQ[0] - iQ[1] * iQ[1] - iQ[2] * iQ[2];
	if ( w > 0 ) iQ[3] = (int)floor( sqrt( (double)w ) + 0.5 );
	else iQ[3] = 0;

	assert( iQ[3] == (short) iQ[3] );
	iQuat[3] = (short) iQ[3];
}

/*
=============
SkelModel::WriteQuat
=============
*/
void SkelModel::WriteQuat( const short *q ) {
	short out[3];

	out[0] = q[0];
	out[1] = q[1];
	out[2] = q[2];
	if ( q[3] < 0 ) {
		out[0] = -out[0];
		out[1] = -out[1];
		out[2] = -out[2];
	}
	SafeWrite( file, out, sizeof( out ) );
}

#line 1046
/*
=============
SkelModel::WriteBlend
=============
*/
void SkelModel::WriteBlend( const Blend *blend, bool noWeight, const vec3_t offset ) {
	short sBone;

	assert( blend->bone == (short) blend->bone );
	sBone = (short)blend->bone;
	SafeWrite( file, &sBone, 2 );
	WriteVec3( offset );
	if ( !noWeight ) {
		float weight = blend->weight;
		SafeWrite( file, &weight, 4 );
	}
}

// Copy an image into the output tree.
/*
=============
SkelModel::CopyFile
=============
*/
void SkelModel::CopyFile( const char *srcFilename, const char *destFilename ) {
	FILE *src, *dest;
	long size;
	char *buffer;
	LPVOID message;

	src = fopen( srcFilename, "rb" );
	if ( !src ) {
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPTSTR)&message, 0, NULL );
		fclose( file );
		Error( "could not open source file '%s', reason: '%s'", srcFilename, message );
		return;
	}
	fseek( src, 0, SEEK_END );
	size = ftell( src );
	fseek( src, 0, SEEK_SET );
	buffer = (char *)malloc( size );
	if ( fread( buffer, 1, size, src ) != size ) {
		fclose( file );
		Error( "Short read in SkelModel::CopyFile" );
		return;
	}
	fclose( src );

	dest = fopen( destFilename, "wb" );
	if ( !dest ) {
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPTSTR)&message, 0, NULL );
		free( buffer );
		fclose( file );
		Error( "could not open destination file '%s', reason: '%s'", destFilename, message );
		return;
	}
	if ( fwrite( buffer, 1, size, dest ) != size ) {
		fclose( file );
		Error( "Short write in SkelModel::CopyFile" );
		return;
	}
	fclose( dest );
	free( buffer );
}

// Resolve the optional language component between the first and last dot.
/*
=============
SkelModel::ImageLanguage
=============
*/
const char *SkelModel::ImageLanguage( const char *filename, int *language ) {
	const char *languages[] = {
		"english", "french", "german", "italian", "spanish", "british", "russian", "polish",
		"korean", "taiwanese", "japanese", "chinese", "thai", "leet"
	};
	const char *first, *last;
	int i, length;

	*language = 0;
	last = strrchr( filename, '.' );
	first = strchr( filename, '.' );
	if ( !last || !first ) {
		return 0;
	}
	if ( last == first ) {
		return languages[0];
	}
	filename = first + 1;
	length = last - filename;
	if ( length < 1 ) {
		return languages[0];
	}
	for ( i = 0; i < 14; i++ ) {
		if ( strlen( languages[i] ) == length && !strnicmp( languages[i], filename, length ) ) {
			*language = i;
			return languages[i];
		}
	}
	return 0;
}

/*
=============
SkelModel::StripImageLanguage
=============
*/
char *SkelModel::StripImageLanguage( const char *filename ) {
	static char stripped[MAX_SKIN_LEN];
	const char *first, *last;

	last = strrchr( filename, '.' );
	first = strchr( filename, '.' );
	if ( !last || !first || last == first ) {
		strcpy( stripped, filename );
		return stripped;
	}
	strncpy( stripped, filename, first - filename );
	stripped[first - filename] = 0;
	strcat( stripped, last );
	return stripped;
}

// Register and copy a surface image, including wildcard families and localized images.
#line 1237
/*
=============
SkelModel::CopySurfaceImage
=============
*/
void SkelModel::CopySurfaceImage( ModelSurface *surf ) {
	char *imageName;
	char srcFilename[MAX_SKIN_LEN];
	char destFilename[MAX_SKIN_LEN];
	char prefix[MAX_SKIN_LEN];
	char skinBase[MAX_SKIN_LEN];
	char text[MAX_SKIN_LEN];
	char wildcard[MAX_SKIN_LEN];
	WIN32_FIND_DATA find;
	HANDLE handle;
	const char *language;
	int languageIndex;
	char *hash;
	char *pszExtension;
	int length;

	if ( model.materialNames[surf->material].copied ) {
		return;
	}
	model.materialNames[surf->material].copied = true;
	imageName = model.materialNames[surf->material].name;

	if ( !stricmp( imageName, "blank.jpg" ) ) {
		sprintf( srcFilename, "%s%s", targetRoot, skinDir );
		assert( !strcmp(srcFilename + strlen(srcFilename) - 7, "/skins/") );
		strcpy( srcFilename + strlen( srcFilename ) - 7, "/textures/common/" );
		strcat( srcFilename, imageName );
		assert( strlen(srcFilename) < MAX_SKIN_LEN );
		sprintf( destFilename, "%s%s%s", targetRoot, skinDir, imageName );
		assert( strlen(destFilename) < MAX_SKIN_LEN );
		convertValue->AddSourceFile( srcFilename );
		convertValue->AddTargetFile( destFilename );
		printf( "copying '%s'\n", imageName );
		CopyFile( srcFilename, destFilename );
		return;
	}

	hash = strchr( imageName, '#' );
	pszExtension = strrchr( imageName, '.' );
#line 1284
	assert( pszExtension );
	length = pszExtension - imageName;
	if ( hash && hash[1] ) {
		length = hash - imageName + 1;
		memcpy( prefix, imageName, length );
		prefix[length] = 0;
		sprintf( wildcard, "%s%s*", sourceDir, prefix );
		convertValue->AddSourceFile( wildcard );
		sprintf( srcFilename, "%s%s*.*%s", sourceDir, prefix, pszExtension );
		handle = FindFirstFile( srcFilename, &find );
		if ( handle == INVALID_HANDLE_VALUE ) {
			sprintf( text, "%s%s%s*", targetRoot, skinDir, prefix );
			convertValue->AddTargetFile( text );
			sprintf( srcFilename, "%s%s*%s", sourceDir, prefix, pszExtension );
			handle = FindFirstFile( srcFilename, &find );
			if ( handle == INVALID_HANDLE_VALUE ) {
				Error( "no files found matching '%s'\n", wildcard );
			}
			do {
				sprintf( srcFilename, "%s%s", sourceDir, find.cFileName );
				sprintf( destFilename, "%s%s%s", targetRoot, skinDir, find.cFileName );
				convertValue->AddTargetFile( destFilename );
				printf( "copying '%s'\n", find.cFileName );
				CopyFile( srcFilename, destFilename );
			} while ( FindNextFile( handle, &find ) );
			FindClose( handle );
			return;
		}
		FindClose( handle );
		bool languageUsed[14];

		memset( languageUsed, 0, sizeof( languageUsed ) );
		strcpy( skinBase, targetRoot );
		skinBase[strlen( targetRoot ) - 1] = 0;
		strcat( skinBase, "_" );
		language = ImageLanguage( imageName, &languageIndex );
		if ( !language || languageIndex ) {
			Error( "language should not be specified for image references by the xmodel: %s", imageName );
		}
		sprintf( srcFilename, "%s%s*%s", sourceDir, prefix, pszExtension );
		handle = FindFirstFile( srcFilename, &find );
		if ( handle == INVALID_HANDLE_VALUE ) {
			Error( "no files found matching '%s'\n", wildcard );
		}
		do {
			sprintf( srcFilename, "%s%s", sourceDir, find.cFileName );
			language = ImageLanguage( find.cFileName, &languageIndex );
			if ( !language ) {
				Error( "bad language for image '%s'\n", srcFilename );
			}
			sprintf( destFilename, "%s%s/%s%s", skinBase, language, skinDir, StripImageLanguage( find.cFileName ) );
			if ( !languageUsed[languageIndex] ) {
				sprintf( text, "%s%s/%s%s*", skinBase, language, skinDir, prefix );
				convertValue->AddTargetFile( text );
				languageUsed[languageIndex] = true;
			}
			convertValue->AddTargetFile( destFilename );
			printf( "copying '%s'\n", find.cFileName );
			CopyFile( srcFilename, destFilename );
		} while ( FindNextFile( handle, &find ) );
		FindClose( handle );
		return;
	}

	memcpy( prefix, imageName, length );
	prefix[length] = 0;
	sprintf( wildcard, "%s%s.*", sourceDir, prefix );
	convertValue->AddSourceFile( wildcard );
	sprintf( srcFilename, "%s%s.*%s", sourceDir, prefix, pszExtension );
	handle = FindFirstFile( srcFilename, &find );
	if ( handle == INVALID_HANDLE_VALUE ) {
		sprintf( text, "%s%s%s.*", targetRoot, skinDir, prefix );
		convertValue->AddTargetFile( text );
		sprintf( srcFilename, "%s%s", sourceDir, imageName );
		sprintf( destFilename, "%s%s%s", targetRoot, skinDir, imageName );
		convertValue->AddTargetFile( destFilename );
		printf( "copying '%s'\n", imageName );
		CopyFile( srcFilename, destFilename );
		return;
	}
	bool languageUsed[14];

	memset( languageUsed, 0, sizeof( languageUsed ) );
	strcpy( skinBase, targetRoot );
	skinBase[strlen( targetRoot ) - 1] = 0;
	strcat( skinBase, "_" );
	language = ImageLanguage( imageName, &languageIndex );
	if ( !language || languageIndex ) {
		Error( "language should not be specified for image references by the xmodel: %s", imageName );
	}
	sprintf( text, "%s%s/%s%s.*", skinBase, language, skinDir, prefix );
	convertValue->AddTargetFile( text );
	languageUsed[languageIndex] = true;
	sprintf( srcFilename, "%s%s", sourceDir, imageName );
	sprintf( destFilename, "%s%s/%s%s", skinBase, language, skinDir, imageName );
	convertValue->AddTargetFile( destFilename );
	printf( "copying '%s'\n", imageName );
	CopyFile( srcFilename, destFilename );
	do {
		sprintf( srcFilename, "%s%s", sourceDir, find.cFileName );
		language = ImageLanguage( find.cFileName, &languageIndex );
		if ( !language ) {
			Error( "bad language for image '%s'\n", srcFilename );
		}
		if ( !languageUsed[languageIndex] ) {
			sprintf( text, "%s%s/%s%s.*", skinBase, language, skinDir, prefix );
			convertValue->AddTargetFile( text );
			languageUsed[languageIndex] = true;
		}
		sprintf( destFilename, "%s%s/%s%s", skinBase, language, skinDir, imageName );
		convertValue->AddTargetFile( destFilename );
		printf( "copying '%s'\n", find.cFileName );
		CopyFile( srcFilename, destFilename );
	} while ( FindNextFile( handle, &find ) );
	FindClose( handle );
}

// Write strip indices, vertex attributes and bone blends for one surface.
#line 1461
/*
=============
SkelModel::WriteSurface
=============
*/
void SkelModel::WriteSurface( ModelSurface *surf ) {
	unsigned short triIndices[MAX_TRIANGLES * 3];
	int invMap[MAX_SURF_VERTS];
	int uniqueVerts[MAX_SURF_VERTS];
	unsigned short remap[MAX_SURF_VERTS];
	triangleIndexGroup_t *groups;
	char *imageName;
	unsigned short *indices;
	SurfVert *vert;
	Blend *blend;
	BonePose *pose;
	float mat[3][3];
	vec3_t offset, local;
	float st[2];
	short sNumverts, sNumtris, sNumStrips, sBoneIndex, sNumblends, sNumSimpleVerts, vertBlends, sIndex;
	char tile, cIndexCount;
	int numStrips, numStripTris, numUniqueVerts, numblends, numSimpleVerts, iIndexCount;
	int i, j, k, vertIndex;

	imageName = model.materialNames[surf->material].name;
	CopySurfaceImage( surf );
	if ( !surf->numtris ) {
		fclose( file );
		Error( "surface '%s' has no tris\n", imageName );
	}

	for ( i = 0, indices = triIndices; i < surf->numtris; i++, indices += 3 ) {
		indices[0] = surf->tris[i].index[0];
		indices[1] = surf->tris[i].index[1];
		indices[2] = surf->tris[i].index[2];
		assert( indices[0] != indices[1] && indices[0] != indices[2] && indices[1] != indices[2] );
	}

	numStrips = GenerateTriangleStripGroups( triIndices, surf->numtris * 3, &groups, remap, 1 );
	numStripTris = 0;
	for ( i = 0; i < numStrips; i++ ) {
		iIndexCount = groups[i].numIndices;
		if ( iIndexCount > 2 ) {
			indices = groups[i].indices;
			for ( j = 2; j < iIndexCount; j++ ) {
				if ( indices[j - 2] != indices[j - 1] && indices[j - 2] != indices[j] && indices[j - 1] != indices[j] ) {
					numStripTris++;
				}
			}
		}
	}
	if ( numStripTris != surf->numtris ) {
		printf( "*** WARNING: tri strip has incorrect number of tris on surface '%s'\n", imageName );
		for ( i = 0; i < numStrips; i++ ) {
			free( groups[i].indices );
		}
		free( groups );
		numStrips = GenerateTriangleListGroups( triIndices, surf->numtris * 3, &groups, remap, surf->numverts );
	}

	for ( i = 0; i < surf->numverts; i++ ) {
		invMap[remap[i]] = i;
	}
	numUniqueVerts = 0;
	for ( i = 0; i < surf->numverts; i++ ) {
		vertIndex = surf->verts[invMap[i]].index;
		for ( j = 0; j < numUniqueVerts; j++ ) {
			if ( uniqueVerts[j] == vertIndex ) {
				break;
			}
		}
		if ( j == numUniqueVerts ) {
			uniqueVerts[numUniqueVerts++] = vertIndex;
		}
	}
	surf->numUniqueVerts = numUniqueVerts;
	surf->numStrips = numStrips;

	tile = surf->state;
	SafeWrite( file, &tile, 1 );
#line 1557
	assert( surf->numverts == (short) surf->numverts );
	sNumverts = surf->numverts;
	SafeWrite( file, &sNumverts, 2 );
#line 1561
	assert( surf->numtris == (short) surf->numtris );
	sNumtris = surf->numtris;
	SafeWrite( file, &sNumtris, 2 );
#line 1565
	assert( numStrips == (short) numStrips );
	sNumStrips = numStrips;
	SafeWrite( file, &sNumStrips, 2 );
#line 1569
	assert( surf->boneIndex == (short) surf->boneIndex );
	sBoneIndex = surf->boneIndex;
	SafeWrite( file, &sBoneIndex, 2 );

	if ( surf->boneIndex == -1 ) {
		numblends = 0;
		vert = surf->verts;
		for ( i = 0; i < surf->numverts; i++, vert++ ) {
#line 1580
			assert( model.verts[vert->index].numbones > 0 );
			numblends += model.verts[vert->index].numbones - 1;
		}
#line 1584
		assert( numblends == (short) numblends );
		sNumblends = numblends;
		SafeWrite( file, &sNumblends, 2 );

		numSimpleVerts = 0;
		vert = surf->verts;
		for ( i = 0; i < surf->numverts; i++, vert++ ) {
#line 1593
			assert( model.verts[vert->index].numbones > 0 );
			if ( model.verts[vert->index].numbones == 1 ) {
				numSimpleVerts++;
			}
		}
		assert( numSimpleVerts == (short) numSimpleVerts );
		sNumSimpleVerts = numSimpleVerts;
		SafeWrite( file, &sNumSimpleVerts, 2 );
	}

	for ( i = 0; i < numStrips; i++ ) {
		iIndexCount = groups[i].numIndices;
#line 1607
		assert( iIndexCount == (unsigned char) iIndexCount );
		cIndexCount = iIndexCount;
		SafeWrite( file, &cIndexCount, 1 );
		for ( j = 0; j < iIndexCount; j++ ) {
			sIndex = groups[i].indices[j];
			SafeWrite( file, &sIndex, 2 );
		}
	}

	for ( i = 0; i < surf->numverts; i++ ) {
		vert = &surf->verts[invMap[i]];
		WriteVec3( vert->normal );
		st[0] = vert->st[0];
		st[1] = vert->st[1];
		SafeWrite( file, st, sizeof( st ) );

		blend = model.verts[vert->index].blend;
		pose = &model.poses->pose[blend->bone];
		offset[0] = model.verts[vert->index].pos[0] - pose->origin[0];
		offset[1] = model.verts[vert->index].pos[1] - pose->origin[1];
		offset[2] = model.verts[vert->index].pos[2] - pose->origin[2];
		MatrixTranspose( pose->axis, mat );
		MatrixTransformVector( offset, mat, local );
#line 1635
		assert( surf->boneIndex == -1 || model.verts[vert->index].numbones == 1 );
		if ( surf->boneIndex == -1 ) {
#line 1638
			assert( model.verts[vert->index].numbones > 0 );
			assert( model.verts[vert->index].numbones == (short) model.verts[vert->index].numbones );
			vertBlends = model.verts[vert->index].numbones - 1;
			SafeWrite( file, &vertBlends, 2 );
#line 1643
			assert( model.verts[vert->index].numbones != 1 || model.verts[vert->index].blend[0].weight == 1.f );
			WriteBlend( blend, model.verts[vert->index].numbones == 1, local );
		} else {
			WriteVec3( local );
		}
	}

	for ( i = 0; i < surf->numverts; i++ ) {
		vert = &surf->verts[invMap[i]];
#line 1656
		assert( model.verts[vert->index].numbones > 0 );
		for ( k = 1; k < model.verts[vert->index].numbones; k++ ) {
			blend = &model.verts[vert->index].blend[k];
			pose = &model.poses->pose[blend->bone];
			offset[0] = model.verts[vert->index].pos[0] - pose->origin[0];
			offset[1] = model.verts[vert->index].pos[1] - pose->origin[1];
			offset[2] = model.verts[vert->index].pos[2] - pose->origin[2];
			MatrixTranspose( pose->axis, mat );
			MatrixTransformVector( offset, mat, local );
			WriteBlend( blend, false, local );
		}
	}

	for ( i = 0; i < numStrips; i++ ) {
		free( groups[i].indices );
	}
	free( groups );
}

// Write the hierarchy and local base pose, followed by bone names and bounds.
#line 1675
/*
=============
SkelModel::WriteParts
=============
*/
void SkelModel::WriteParts( const char *filename ) {
	ModelBone *boneName;
	BonePose *parentPose, *pose;
	float parentAxis[4][3], localAxis[3][3];
	vec3_t pos, offset;
	float quat[4];
	short iQuat[4];
	short numBones;
	int iParent;
	unsigned char classification;
	int version;
	int numRootBones;
	int parentIndex;
	int i, j;

	Swap_Init();
	assert( model.numframes );
	file = SafeOpenWrite( filename );
	version = XMODEL_VERSION;
	SafeWrite( file, &version, 2 );
	numRootBones = 0;
	for ( i = 0; i < model.numbones; i++ ) {
		if ( model.bones[i].parent != -1 ) {
			break;
		}
		numRootBones++;
	}
#line 1712
	assert( model.numbones == (short) model.numbones );
	numBones = model.numbones - numRootBones;
	SafeWrite( file, &numBones, 2 );
	SafeWrite( file, &numRootBones, 2 );
	for ( j = (short)numRootBones; j < model.numbones; j++ ) {
		parentIndex = model.bones[j].parent;
#line 1726
		assert( parentIndex >= 0 );
		assert( parentIndex < j );
		assert( parentIndex == (unsigned char) parentIndex );
		iParent = (short)parentIndex;
		SafeWrite( file, &iParent, 1 );
		parentPose = &model.poses->pose[parentIndex];
		pose = &model.poses->pose[j];
		MatrixTranspose( parentPose->axis, parentAxis );
		pos[0] = parentPose->origin[0] * -1;
		pos[1] = parentPose->origin[1] * -1;
		pos[2] = parentPose->origin[2] * -1;
		MatrixTransformVector( pos, parentAxis, parentAxis[3] );
		MatrixMultiply( pose->axis, parentAxis, localAxis );
		if ( writeOffsets ) {
			MatrixTransformVector43( pose->origin, parentAxis, offset );
		} else {
			offset[0] = offset[1] = offset[2] = 0;
		}
		WriteVec3( offset );
		MatrixToQuat( localAxis, quat );
		QuatToShort( quat, iQuat );
		WriteQuat( iQuat );
	}
	for ( i = 0; i < model.numbones; i++ ) {
		boneName = &model.bones[i];
		SafeWrite( file, boneName->name, strlen( boneName->name ) + 1 );
		WriteVec3( boneName->mins );
		WriteVec3( boneName->maxs );
	}
	for ( i = 0; i < model.numbones; i++ ) {
		boneName = &model.bones[i];
		classification = boneName->classification;
#line 1770
		assert( boneName->classification == classification );
		SafeWrite( file, &classification, 1 );
	}
	fclose( file );
}

// Prefer dds, tga, then jpg when resolving a material image.
/*
=============
SkelModel::FindOrAddImage
=============
*/
MaterialName *SkelModel::FindOrAddImage( const char *name ) {
	char base[MAX_SKIN_LEN];
	char filename[MAX_SKIN_LEN];
	char path[MAX_SKIN_LEN];
	struct _stat st;
	MaterialName *image;
	int length;

	length = strlen( name );
	if ( name[length - 4] == '.' ) {
		length -= 4;
	}
	memcpy( base, name, length );
	base[length] = 0;
	sprintf( filename, "%s.dds", base );
	sprintf( path, "%s%s", sourceDir, filename );
	if ( _stat( path, &st ) ) {
		sprintf( filename, "%s.tga", base );
		sprintf( path, "%s%s", sourceDir, filename );
		if ( _stat( path, &st ) ) {
			sprintf( filename, "%s.jpg", base );
			sprintf( path, "%s%s", sourceDir, filename );
			if ( _stat( path, &st ) ) {
				sprintf( path, "%s%s.*", sourceDir, base );
				Error( "Could not find image file '%s'\n", path );
			}
		}
	}
	image = FindImage( filename );
	if ( !image ) {
		image = &model.materialNames[model.numMaterials++];
		memset( image->name, 0, sizeof( image->name ) );
		strncpy( image->name, filename, sizeof( image->name ) - 1 );
	}
	return image;
}

// Write the surface file and report strip and blended-vertex statistics.
#line 1896
/*
=============
SkelModel::WriteSurfaces
=============
*/
void SkelModel::WriteSurfaces( const char *filename ) {
	ModelSurface *surf;
	short numsurfaces;
	int version;
	int numtris;
	int numPhysiqued, numPhysiquedVerts, numPhysiquedUniqueVerts;
	int i;

	Swap_Init();
	if ( !model.numsurfaces ) {
		Error( "xmodel '%s' has no surfaces", filename );
	}
	numtris = 0;
	surf = model.surfaces;
	for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
		numtris += surf->numtris;
	}
	assert( model.numfaces == numtris + model.unusedFaces );
	file = SafeOpenWrite( filename );
	version = XMODEL_VERSION;
	SafeWrite( file, &version, 2 );
#line 1925
	assert( model.numsurfaces == (short) model.numsurfaces );
	numsurfaces = model.numsurfaces;
	SafeWrite( file, &numsurfaces, 2 );
	printf( "src skin dir '%s'\n", sourceDir );
	printf( "trg skin dir '%s%s'\n", targetRoot, skinDir );
	surf = model.surfaces;
	for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
		WriteSurface( surf );
	}
#line 1936
	assert( model.unusedFaces >= 0 );
	if ( model.unusedFaces ) {
		printf( "*** %d tris discarded due to no material specified ***\n", model.unusedFaces );
	}
	printf( "\naverage strip lengths:\n" );
	surf = model.surfaces;
	for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
		printf( "%4.1f %s (%d tris, %d verts, %d unique verts%s)\n", (double)surf->numtris / surf->numStrips,
			model.materialNames[surf->material].name, surf->numtris, surf->numverts, surf->numUniqueVerts,
			surf->boneIndex == -1 ? ", physiqued" : "" );
	}
	surf = model.surfaces;
	for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
		if ( surf->boneIndex == -1 ) {
			numPhysiqued = 0;
			numPhysiquedVerts = 0;
			numPhysiquedUniqueVerts = 0;
			printf( "\nphysiqued verts:\n" );
			surf = model.surfaces;
			for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
				if ( surf->boneIndex == -1 ) {
					numPhysiqued += surf->numtris;
					numPhysiquedVerts += surf->numverts;
					numPhysiquedUniqueVerts += surf->numUniqueVerts;
				}
			}
			printf( "%d tris, %d verts, %d unique verts\n", numPhysiqued, numPhysiquedVerts, numPhysiquedUniqueVerts );
			break;
		}
	}
	fclose( file );
}

// Write LOD references, bounds, collision surfaces and the skin names of each LOD.
#line 1993
/*
=============
WriteXModel
=============
*/
void WriteXModel( const char *filename, ModelLods *lods, SurfaceNames *surfNames ) {
	FILE *file;
	CollSurface *collSurf;
	CollTri *tri;
	float v[4];
	vec3_t v3;
	float value;
	float dist;
	int collisionLod;
	int iValue;
	short sNumsurfaces;
	int version;
	int i, j;

	Swap_Init();
	file = SafeOpenWrite( filename );
	version = XMODEL_VERSION;
	SafeWrite( file, &version, 2 );
	value = lods->mins[0];
	SafeWrite( file, &value, 4 );
	value = lods->mins[1];
	SafeWrite( file, &value, 4 );
	value = lods->mins[2];
	SafeWrite( file, &value, 4 );
	value = lods->maxs[0];
	SafeWrite( file, &value, 4 );
	value = lods->maxs[1];
	SafeWrite( file, &value, 4 );
	value = lods->maxs[2];
	SafeWrite( file, &value, 4 );
	for ( i = 0; i < MAX_LODS; i++ ) {
		dist = lods->lods[i].distance;
		SafeWrite( file, &dist, 4 );
		SafeWrite( file, lods->lods[i].filename, strlen( lods->lods[i].filename ) + 1 );
	}
	collisionLod = lods->collisionLod;
	SafeWrite( file, &collisionLod, 4 );
	iValue = collmodel.numCollSurfs;
	SafeWrite( file, &iValue, 4 );
	for ( i = 0; i < collmodel.numCollSurfs; i++ ) {
		collSurf = &collmodel.collSurfs[i];
		iValue = collSurf->numCollTris;
		SafeWrite( file, &iValue, 4 );
		for ( j = 0; j < collSurf->numCollTris; j++ ) {
			tri = &collSurf->tris[j];
			v[0] = tri->plane[0];
			v[1] = tri->plane[1];
			v[2] = tri->plane[2];
			v[3] = tri->plane[3];
			SafeWrite( file, v, 16 );
			v[0] = tri->svec[0];
			v[1] = tri->svec[1];
			v[2] = tri->svec[2];
			v[3] = tri->svec[3];
			SafeWrite( file, v, 16 );
			v[0] = tri->tvec[0];
			v[1] = tri->tvec[1];
			v[2] = tri->tvec[2];
			v[3] = tri->tvec[3];
			SafeWrite( file, v, 16 );
		}
		v3[0] = collSurf->mins[0];
		v3[1] = collSurf->mins[1];
		v3[2] = collSurf->mins[2];
		SafeWrite( file, v3, 12 );
		v3[0] = collSurf->maxs[0];
		v3[1] = collSurf->maxs[1];
		v3[2] = collSurf->maxs[2];
		SafeWrite( file, v3, 12 );
		iValue = collSurf->boneIdx;
		SafeWrite( file, &iValue, 4 );
		iValue = collSurf->contents;
		SafeWrite( file, &iValue, 4 );
		iValue = collSurf->surfaceFlags;
		SafeWrite( file, &iValue, 4 );
	}
	for ( i = 0; i < MAX_LODS; i++ ) {
		if ( !lods->lods[i].filename[0] ) {
			break;
		}
		sNumsurfaces = surfNames[i].num;
		assert( sNumsurfaces == surfNames[i].num );
		SafeWrite( file, &sNumsurfaces, 2 );
		for ( j = 0; j < surfNames[i].num; j++ ) {
			SafeWrite( file, surfNames[i].names[j], strlen( surfNames[i].names[j] ) + 1 );
		}
	}
	fclose( file );
}

// Resolve object/material overrides and collect the resulting surface image names.
#line 1782
/*
=============
SkelModel::ApplySkinOverrides
=============
*/
void SkelModel::ApplySkinOverrides( SurfaceNames *surfNames, SkinOverride *overrides, int numOverrides ) {
	int *overrideTable;
	ModelSurface *surf;
	MaterialName *image;
	const char *skinName;
	int imageIndex;
	int i, j, k, o, index;

	overrideTable = (int *)alloca( model.numObjects * model.numMaterials * sizeof( int ) );
	for ( i = 0; i < model.numMaterials; i++ ) {
		for ( j = 0; j < model.numObjects; j++ ) {
			overrideTable[model.numObjects * i + j] = -1;
		}
	}
	for ( k = 0; k < numOverrides; k++ ) {
		image = FindImage( overrides[k].material );
		if ( !image ) {
			continue;
		}
		imageIndex = image - model.materialNames;
		for ( j = 0; j < model.numObjects; j++ ) {
			if ( overrides[k].object[0] && stricmp( model.objectNames[j], overrides[k].object ) ) {
				continue;
			}
			index = model.numObjects * imageIndex + j;
			o = overrideTable[index];
			if ( o >= 0 ) {
				Error( "skinOverride '%s' cannot be applied due to conflict with previous override '%s'", overrides[k].skin, overrides[o].skin );
			}
			overrideTable[index] = k;
		}
	}
	assert( model.numsurfaces < MAX_SURFACES );
	surfNames->num = 0;
	surf = model.surfaces;
	for ( i = 0; i < model.numsurfaces; i++, surf++ ) {
		skinName = model.materialNames[surf->material].name;
		if ( stricmp( skinName, "blank.jpg" ) ) {
			index = model.numObjects * surf->material + surf->object;
			if ( overrideTable[index] >= 0 ) {
				skinName = overrides[overrideTable[index]].skin;
				overrides[overrideTable[index]].used = true;
			}
			image = FindOrAddImage( skinName );
			skinName = image->name;
			surf->material = image - model.materialNames;
		}
		surfNames->names[i] = (char *)malloc( strlen( skinName ) + 1 );
		strcpy( surfNames->names[i], skinName );
#line 1841
		assert( i == surfNames->num );
		surfNames->num++;
	}
}

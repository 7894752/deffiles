/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>

#include "../libs/nvtristrip/NvTriStrip.h"

#include "tristrip.h"

/*
=============
GenerateTriangleListGroups
=============
*/
extern "C" int GenerateTriangleListGroups( const unsigned short *indices,
	unsigned short numIndices, triangleIndexGroup_t **groups,
	unsigned short *remap, unsigned short numVerts ) {
	unsigned short numGroups;
	int i;

	numGroups = numIndices / 3;
	*groups = (triangleIndexGroup_t *)malloc( numGroups * sizeof( **groups ) );
	for ( i = 0; i < numGroups; i++ ) {
		(*groups)[i].numIndices = 3;
		(*groups)[i].indices = (unsigned short *)malloc( (*groups)[i].numIndices * sizeof( *indices ) );
		(*groups)[i].indices[0] = *indices++;
		(*groups)[i].indices[1] = *indices++;
		(*groups)[i].indices[2] = *indices++;
	}
	for ( i = 0; i < numVerts; i++ ) {
		remap[i] = i;
	}
	return numGroups;
}

/*
=============
GenerateTriangleStripGroups
=============
*/
int GenerateTriangleStripGroups( const unsigned short *indices,
	unsigned short numIndices, triangleIndexGroup_t **groups,
	unsigned short *remap, int useVertexCache ) {
	PrimitiveGroup *primitiveGroups, *remappedGroups;
	unsigned short numGroups, numVerts;
	int i;
	unsigned int j;

	if ( useVertexCache ) {
		SetCacheSize( 16 );
	} else {
		SetCacheSize( 4096 );
	}
	SetStitchStrips( false );
	SetMinStripSize( 0 );
	SetListsOnly( false );
	GenerateStrips( indices, numIndices, &primitiveGroups, &numGroups );
	numVerts = 0;
	for ( i = 0; i < numGroups; i++ ) {
		for ( j = 0; j < primitiveGroups[i].numIndices; j++ ) {
			if ( numVerts <= primitiveGroups[i].indices[j] ) {
				numVerts = primitiveGroups[i].indices[j] + 1;
			}
		}
	}
	RemapIndices( primitiveGroups, numGroups, numVerts, &remappedGroups );
	*groups = (triangleIndexGroup_t *)malloc( numGroups * sizeof( **groups ) );
	for ( i = 0; i < numGroups; i++ ) {
		(*groups)[i].numIndices = remappedGroups[i].numIndices;
		(*groups)[i].indices = (unsigned short *)malloc( (*groups)[i].numIndices * sizeof( *indices ) );
		for ( j = 0; j < remappedGroups[i].numIndices; j++ ) {
			(*groups)[i].indices[j] = remappedGroups[i].indices[j];
			remap[primitiveGroups[i].indices[j]] = remappedGroups[i].indices[j];
		}
	}
	delete[] primitiveGroups;
	delete[] remappedGroups;
	return numGroups;
}

/*
=============
RemapTriangleListIndices
=============
*/
void RemapTriangleListIndices( unsigned short *indices, unsigned short numIndices,
	unsigned short *remap ) {
	PrimitiveGroup *groups, *remappedGroups;
	unsigned short numGroups, numVerts;
	unsigned int i;

	SetCacheSize( 16 );
	SetStitchStrips( false );
	SetMinStripSize( 0 );
	SetListsOnly( true );
	GenerateStrips( indices, numIndices, &groups, &numGroups );
	if ( numGroups != 1 ) {
		printf( "WARNING: GenerateStrips returned multiple lists\n" );
		return;
	}

	numVerts = 0;
	for ( i = 0; i < groups->numIndices; i++ ) {
		if ( numVerts <= groups->indices[i] ) {
			numVerts = groups->indices[i] + 1;
		}
	}
	RemapIndices( groups, 1, numVerts, &remappedGroups );
	PrimitiveGroup result = *remappedGroups;
	for ( i = 0; i < result.numIndices; i++ ) {
		indices[i] = result.indices[i];
		remap[groups->indices[i]] = result.indices[i];
	}
}

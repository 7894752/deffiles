/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include "assertive.h"

typedef float vec_t;
typedef vec_t vec3_t[3];

extern "C" {
double acos( double x );
vec_t VectorNormalize( vec3_t v );
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );
void PerpendicularVector( vec3_t dst, const vec3_t src );
}

#define DotProduct( a, b )	( (a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] )

#define MAX_TANGENT_SPACE_VERTS	0x10000

// Strided vertex streams: positions, normals and texture coordinates in, tangents and binormals out.
struct TangentSpaceSources {
	const float	*xyz, *normal, *st;
	float		*tangent, *binormal;
	int			xyzStride, normalStride, stStride;
	int			tangentStride, binormalStride;
};

#define TS_VEC( base, stride, index )	( (float *)( (char *)( base ) + ( stride ) * ( index ) ) )

/*
=============
TriangleTangents
=============
*/
static void TriangleTangents( const float *xyz0, const float *xyz1, const float *xyz2,
	const float *st0, const float *st1, const float *st2, vec3_t tangent, vec3_t binormal ) {
	float	ds1, dt1, ds2, dt2, edge1, edge2;
	int		k;
	ds1 = st1[0] - st0[0];
	ds2 = st2[0] - st0[0];
	dt1 = st1[1] - st0[1];
	dt2 = st2[1] - st0[1];
	for ( k = 0; k < 3; k++ ) {
		edge1 = xyz1[k] - xyz0[k];
		edge2 = xyz2[k] - xyz0[k];
		if ( ds1 * dt2 > ds2 * dt1 ) {
			tangent[k] = edge1 * dt2 - edge2 * dt1;
			binormal[k] = edge2 * ds1 - edge1 * ds2;
		} else {
			tangent[k] = edge2 * dt1 - edge1 * dt2;
			binormal[k] = edge1 * ds2 - edge2 * ds1;
		}
	}
	VectorNormalize( tangent );
	VectorNormalize( binormal );
}

/*
=============
AngleBetweenNormals
=============
*/
static float AngleBetweenNormals( const vec3_t a, const vec3_t b ) {
	float	cosine;
	cosine = DotProduct( a, b );
	if ( cosine <= -1.0f ) return 3.14159265f;
	if ( cosine >= 1.0f ) return 0.0f;
	return (float)acos( cosine );
}

// The angle of the triangle at each of its corners, used as the weight of that corner.
/*
=============
CornerAngles
=============
*/
static void CornerAngles( const TangentSpaceSources *sources, int i0, int i1, int i2, vec3_t angles ) {
	vec3_t	edge01, edge12, edge20;
	const float	*xyz0, *xyz1, *xyz2;
	int		k;
	xyz0 = TS_VEC( sources->xyz, sources->xyzStride, i0 );
	xyz1 = TS_VEC( sources->xyz, sources->xyzStride, i1 );
	xyz2 = TS_VEC( sources->xyz, sources->xyzStride, i2 );
	for ( k = 0; k < 3; k++ ) {
		edge01[k] = xyz1[k] - xyz0[k];
		edge12[k] = xyz2[k] - xyz1[k];
		edge20[k] = xyz0[k] - xyz2[k];
	}
	VectorNormalize( edge01 );
	VectorNormalize( edge12 );
	VectorNormalize( edge20 );
	angles[0] = AngleBetweenNormals( edge01, edge20 );
	angles[1] = AngleBetweenNormals( edge12, edge01 );
	angles[2] = AngleBetweenNormals( edge20, edge12 );
}

/*
=============
CalculateTangentSpace
=============
*/
void CalculateTangentSpace( const TangentSpaceSources *sources, int vertCount, const unsigned short *indices,
	int indexCount ) {
	vec3_t	triTangent, triBinormal, angles, cross;
	float	*tangent, *binormal;
	const float	*normal;
	float	d;
	int		i, k, i0, i1, i2, corner[3];

	IW_ASSERT( sources );
	IW_ASSERT( indices );
	IW_ASSERT( vertCount <= MAX_TANGENT_SPACE_VERTS );

	for ( i = 0; i < vertCount; i++ ) {
		tangent = TS_VEC( sources->tangent, sources->tangentStride, i );
		binormal = TS_VEC( sources->binormal, sources->binormalStride, i );
		tangent[0] = tangent[1] = tangent[2] = 0;
		binormal[0] = binormal[1] = binormal[2] = 0;
	}

	for ( i = 0; i < indexCount; i += 3 ) {
		i0 = indices[i + 0];
		i1 = indices[i + 1];
		i2 = indices[i + 2];
		IW_ASSERT( i0 >= 0 && i0 < vertCount );
		IW_ASSERT( i1 >= 0 && i1 < vertCount );
		IW_ASSERT( i2 >= 0 && i2 < vertCount );

		TriangleTangents( TS_VEC( sources->xyz, sources->xyzStride, i0 ), TS_VEC( sources->xyz, sources->xyzStride, i1 ),
			TS_VEC( sources->xyz, sources->xyzStride, i2 ), TS_VEC( sources->st, sources->stStride, i0 ),
			TS_VEC( sources->st, sources->stStride, i1 ), TS_VEC( sources->st, sources->stStride, i2 ),
			triTangent, triBinormal );
		CornerAngles( sources, i0, i1, i2, angles );

		corner[0] = i0;
		corner[1] = i1;
		corner[2] = i2;
		for ( k = 0; k < 3; k++ ) {
			tangent = TS_VEC( sources->tangent, sources->tangentStride, corner[k] );
			binormal = TS_VEC( sources->binormal, sources->binormalStride, corner[k] );
			tangent[0] += triTangent[0] * angles[k];
			tangent[1] += triTangent[1] * angles[k];
			tangent[2] += triTangent[2] * angles[k];
			binormal[0] += triBinormal[0] * angles[k];
			binormal[1] += triBinormal[1] * angles[k];
			binormal[2] += triBinormal[2] * angles[k];
		}
	}

	for ( i = 0; i < vertCount; i++ ) {
		normal = TS_VEC( sources->normal, sources->normalStride, i );
		tangent = TS_VEC( sources->tangent, sources->tangentStride, i );
		binormal = TS_VEC( sources->binormal, sources->binormalStride, i );

		// make the tangent perpendicular to the normal
		d = -DotProduct( normal, tangent );
		tangent[0] += d * normal[0];
		tangent[1] += d * normal[1];
		tangent[2] += d * normal[2];
		if ( VectorNormalize( tangent ) < 0.1f ) {
			CrossProduct( binormal, normal, tangent );
			if ( VectorNormalize( tangent ) < 0.1f ) {
				PerpendicularVector( tangent, normal );
			}
		}

		// the binormal completes the frame, keeping the side of the accumulated one
		CrossProduct( normal, tangent, cross );
		if ( DotProduct( cross, binormal ) < 0.0 ) {
			binormal[0] = -cross[0];
			binormal[1] = -cross[1];
			binormal[2] = -cross[2];
		} else {
			binormal[0] = cross[0];
			binormal[1] = cross[1];
			binormal[2] = cross[2];
		}
	}
}

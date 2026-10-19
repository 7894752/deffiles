/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/
// Animation export and frame reduction.
#include "xanim.h"
/*
=============
Animation::WriteVec3
=============
*/
void Animation::WriteVec3( float *v ) {
	float out[3];
	out[0] = LittleFloat( v[0] );
	out[1] = LittleFloat( v[1] );
	out[2] = LittleFloat( v[2] );
	SafeWrite( model.file, out, sizeof( out ) );
}
/*
=============
MatrixToQuat
=============
*/
void MatrixToQuat( float m[3][3], float quat[4] ) {
	float q[4][4], mag[4];
	int best;
	// Use the largest diagonal candidate to avoid division by a small component.
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
/*
=============
QuatToShort
=============
*/
static void QuatToShort( float *quat, short *iQuat ) {
	float q[4];
	float S;
	int iQ[4];
	int w, k;
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
	S = 32767.0 / sqrt( S );

	for ( k = 0; k < 3; k++ ) {
		iQ[k] = (int)floor( S * q[k] + 0.5f );

		assert( iQ[k] == (short) iQ[k] );
		iQuat[k] = (short) iQ[k];
	}

	w = 32767 * 32767 - iQ[0] * iQ[0] - iQ[1] * iQ[1] - iQ[2] * iQ[2];
	if ( w > 0 ) iQ[3] = (int)floor( sqrt( (double)w ) + 0.5 );
	else iQ[3] = 0;

	assert( iQ[3] == (short) iQ[3] );
	iQuat[3] = (short) iQ[3];
}
/*
=============
QuatToShort2
=============
*/
static void QuatToShort2( float *quat, short *iQuat ) {
	float S;
	int iQ, i;
	S = quat[0] * quat[0] + quat[1] * quat[1];
	assert( S );
	S = 32767.0 / sqrt( S );

	for ( i = 0; i < 2; i++ ) {
		iQ = (int)floor( S * quat[i] + 0.5f );

		assert( iQ == (short) iQ );
		iQuat[i] = (short) iQ;
	}
}
/*
=============
Animation::WriteQuatXYZ
=============
*/
void Animation::WriteQuatXYZ( short *iQuat ) {
	short s[3];
	int i;
	s[0] = iQuat[0];
	s[1] = iQuat[1];
	s[2] = iQuat[2];
	if ( iQuat[3] < 0 ) {
		s[0] = -s[0];
		s[1] = -s[1];
		s[2] = -s[2];
	}
	for ( i = 0; i < 3; i++ ) {
		s[i] = LittleShort( s[i] );
	}
	SafeWrite( model.file, s, sizeof( s ) );
}
/*
=============
Animation::WriteQuatZ
=============
*/
void Animation::WriteQuatZ( short *iQuat ) {
	short s;
	s = iQuat[0];
	if ( iQuat[1] < 0 ) {
		s = -s;
	}
	s = LittleShort( s );
	SafeWrite( model.file, &s, sizeof( s ) );
}
// Notetracks are named animation events, such as footsteps or shell ejection.
// The export stores channels of names and frame numbers; the file stores
// one flat list ordered by frame. The count fits in a single byte.
// The channel and frame limits are checked when reading the export.
// Equal frames may fire together, so no stable ordering is needed.
/*
=============
CompareNoteTracks
=============
*/
static int CompareNoteTracks( const void *a, const void *b ) {
	return ( (const noteTrack_t *)a )->frameIndex - ( (const noteTrack_t *)b )->frameIndex;
}
// File layout: byte count, then each zero-terminated name and short frame index.
// The channel name is the event name the game script receives.
// Frame indices are relative to the first frame of the animation.

/*
=============
Animation::WriteNotetracks
=============
*/
void Animation::WriteNotetracks() {
	noteTrack_t noteTracks[MAX_NOTETRACK];
	noteTrack_t *noteTrack;
	NotetrackChannel *channel;
	int totalNotetracks;
	unsigned char count;
	int i, j;
	unsigned short frame;
	// flatten the channels
	totalNotetracks = 0;
	for ( i = 0; i < model.numChannels; i++ ) {
		channel = &model.channels[i];
		for ( j = 0; j < channel->numFrames; j++ ) {
			assert( totalNotetracks < MAX_NOTETRACK );
			noteTrack = &noteTracks[totalNotetracks];
			strcpy( noteTrack->name, channel->name );
			noteTrack->frameIndex = channel->frames[j];
			totalNotetracks++;
		}
	}

	// the runtime walks the list with the current frame, so order matters
	// (a stable order is not needed: equal frames fire together)
	qsort( noteTracks, totalNotetracks, sizeof( noteTrack_t ), CompareNoteTracks );

	assert( totalNotetracks == (unsigned char) totalNotetracks );
	count = (unsigned char) totalNotetracks;
	SafeWrite( model.file, &count, 1 );

	noteTrack = noteTracks;
	for ( i = 0; i < totalNotetracks; i++, noteTrack++ ) {
		SafeWrite( model.file, noteTrack->name, strlen( noteTrack->name ) + 1 );
		// frame indices are stored as 16 bit values
		assert( noteTrack->frameIndex == (unsigned short) noteTrack->frameIndex );
		frame = (unsigned short) noteTrack->frameIndex;
		SafeWrite( model.file, &frame, 2 );
	}
}
// Writes the rotation of one bone for every frame. 
// A frame only needs to be stored if the rotation cannot be interpolated from
// its neighbours within the tolerance, so the frames are reduced to a list of
// keys.  Returns true if the keys only turn around the z axis; the loader gets
// a bit for that and the keys then take 2 bytes instead of 6.
//
// The rotations are first quantized to 16 bit (QuatToShort), because the file
// stores them that way and neighbouring frames are compared on the quantized
// values.  The tolerance is rotationThreshold, an angle in degrees, converted into
// the measure that QuatRatioEigenTrace returns.  _quatFrames is modified: the
// signs are flipped so that consecutive rotations are never on opposite sides.
//
/*
=============
Animation::WriteQuatFrames
=============
*/
bool Animation::WriteQuatFrames( float (*_quatFrames)[4], bool bKeepIdentity ) {
	float tolerance;
	short (*_sQuatFrames)[4];
	short (*quatKeys)[4];
	short *key, *newKey;
	void *quatIndices;
	bool bByteIndices;
	unsigned short numQuatIndices;
	float quat[4];
	float frac;
	int frame, end, i, k;
	tolerance = AngleEigenTrace( model.rotationThreshold );
	_sQuatFrames = (short (*)[4])alloca( model.numframes * sizeof( short[4] ) );
	for ( i = 0; i < model.numframes; i++ ) {
		QuatToShort( _quatFrames[i], _sQuatFrames[i] );
		assert( _quatFrames[i][3] >= 0 );
		assert( _sQuatFrames[i][3] >= 0 );
	}

	// keep neighbouring rotations on the same hemisphere
	for ( k = 1; k < model.numframes; k++ ) {
		int dot = _sQuatFrames[k - 1][0] * _sQuatFrames[k][0];
		dot += _sQuatFrames[k - 1][1] * _sQuatFrames[k][1];
		dot += _sQuatFrames[k - 1][2] * _sQuatFrames[k][2];
		dot += _sQuatFrames[k - 1][3] * _sQuatFrames[k][3];
		if ( dot < 0 ) {
			_sQuatFrames[k][0] = -_sQuatFrames[k][0];
			_sQuatFrames[k][1] = -_sQuatFrames[k][1];
			_sQuatFrames[k][2] = -_sQuatFrames[k][2];
			_sQuatFrames[k][3] = -_sQuatFrames[k][3];
			float *q = _quatFrames[k];
			q[0] = -q[0];
			q[1] = -q[1];
			q[2] = -q[2];
			q[3] = -q[3];
		}
	}

	// Key reduction.  The keys are a list of frames; everything between two
	// neighbouring keys is rebuilt by the loader with a normalized linear
	// interpolation (nlerp) of the two rotations.
	//
	// Starting at a key, look for the farthest frame that can be the next key:
	// every frame in between has to be within the tolerance of the nlerp.
	// End keys must share a hemisphere to avoid the long interpolation.
	//
	// The first frame is always a key.  The index of every key is kept in a
	// separate list so that the loader knows which frames are keyed:
	//	quatKeys[]	the quantized key rotations
	//	quatIndices[]	their frame numbers
	//
	bByteIndices = model.numframes <= 256;
	if ( bByteIndices ) quatIndices = alloca( model.numframes );
	else quatIndices = alloca( model.numframes * 2 );
	quatKeys = (short (*)[4])alloca( model.numframes * sizeof( short[4] ) );

	frame = 0;
	numQuatIndices = 0;
	quatKeys[0][0] = _sQuatFrames[0][0];
	quatKeys[0][1] = _sQuatFrames[0][1];
	quatKeys[0][2] = _sQuatFrames[0][2];
	quatKeys[0][3] = _sQuatFrames[0][3];
	if ( bByteIndices ) {
		( (unsigned char *)quatIndices )[0] = 0;
	} else {
		( (unsigned short *)quatIndices )[0] = 0;
	}

	if ( frame < model.numframes - 1 ) {
		key = _sQuatFrames[0];
		do {
			// extend the segment while every frame inside it stays close to the interpolation
			for ( end = frame + 2; end < model.numframes; end++ ) {
				int dot = _sQuatFrames[end][0] * key[0];
				dot += _sQuatFrames[end][1] * key[1];
				dot += _sQuatFrames[end][2] * key[2];
				dot += _sQuatFrames[end][3] * key[3];
				if ( dot <= 0 ) {
					break;
				}
				for ( k = frame + 1; k < end; k++ ) {
					frac = (float)( k - frame ) / (float)( end - frame );
					quat[0] = _quatFrames[frame][0] + ( _quatFrames[end][0] - _quatFrames[frame][0] ) * frac;
					quat[1] = _quatFrames[frame][1] + ( _quatFrames[end][1] - _quatFrames[frame][1] ) * frac;
					quat[2] = _quatFrames[frame][2] + ( _quatFrames[end][2] - _quatFrames[frame][2] ) * frac;
					quat[3] = _quatFrames[frame][3] + ( _quatFrames[end][3] - _quatFrames[frame][3] ) * frac;
					VectorNormalize4D( quat );
					if ( QuatRatioEigenTrace( quat, _quatFrames[k] ) >= tolerance ) {
						goto nextKey;
					}
				}
			}
nextKey:
			numQuatIndices++;
			frame = end - 1;
			newKey = quatKeys[numQuatIndices];

			key = _sQuatFrames[frame];
			newKey[0] = key[0];
			newKey[1] = key[1];
			newKey[2] = key[2];
			newKey[3] = key[3];
			if ( bByteIndices ) {
				( (unsigned char *)quatIndices )[numQuatIndices] = (unsigned char)frame;
			} else {
				( (unsigned short *)quatIndices )[numQuatIndices] = (unsigned short)frame;
			}
		} while ( frame < model.numframes - 1 );

	}

	if ( !bKeepIdentity ) {
		for ( i = 0; i <= numQuatIndices; i++ ) {
			if ( quatKeys[i][0] != 0 || quatKeys[i][1] != 0 || quatKeys[i][2] != 0 ) {
				break;
			}
		}
		if ( i > numQuatIndices ) {
			// no rotation at all
			numQuatIndices = 0;
			SafeWrite( model.file, &numQuatIndices, 2 );
			return true;
		}
	}

	if ( numQuatIndices == 0 ) {
		numQuatIndices = 1;
		SafeWrite( model.file, &numQuatIndices, 2 );
		if ( quatKeys[0][0] == 0 && quatKeys[0][1] == 0 ) {
			WriteQuatZ( &quatKeys[0][2] );
			return true;
		}
		WriteQuatXYZ( quatKeys[0] );
	} else if ( numQuatIndices == 1 && quatKeys[0][0] == quatKeys[1][0] && quatKeys[0][1] == quatKeys[1][1] &&
		 quatKeys[0][2] == quatKeys[1][2] && quatKeys[0][3] == quatKeys[1][3] ) {
		SafeWrite( model.file, &numQuatIndices, 2 );
		if ( quatKeys[0][0] == 0 && quatKeys[0][1] == 0 ) {
			WriteQuatZ( &quatKeys[0][2] );
			return true;
		}
		WriteQuatXYZ( quatKeys[0] );
	} else {
		// File layout of the rotation of one bone:
		//
		// short: number of keys, 0 for no rotation.
		// When there are fewer keys than frames, the frame numbers follow:
		// a byte per key for short animations, otherwise a short per key.
		//
		// The keys follow their frame numbers.  If all the rotations are
		// around z, each key is a single short; otherwise it is short[3].
		// The w component is implied by the other three components.
		//
		// A single key never has a frame number.  The caller records the
		// rotation encoding in a bit field alongside the bone names.
		//
		// The reduction count excludes the first key; include it in the file.
		// Index data is omitted when every frame is still a key.
		numQuatIndices++;
		SafeWrite( model.file, &numQuatIndices, 2 );
		assert( numQuatIndices <= model.numframes );
		if ( numQuatIndices < model.numframes ) {
			if ( bByteIndices ) SafeWrite( model.file, quatIndices, numQuatIndices );
			else SafeWrite( model.file, quatIndices, numQuatIndices * 2 );
		}
		bool bZOnly = true;
		for ( i = 0; i < numQuatIndices; i++ ) {
			if ( quatKeys[i][0] != 0 || quatKeys[i][1] != 0 ) {
				bZOnly = false;
			}
		}
		if ( !bZOnly ) {
			for ( i = 0; i < numQuatIndices; i++ )
				WriteQuatXYZ( quatKeys[i] );
		} else {
			for ( i = 0; i < numQuatIndices; i++ )
				WriteQuatZ( &quatKeys[i][2] );
			return true;
		}
	}
	return false;
}

// Same reduction for the translation of a bone.  Keys are dropped as long as
// the interpolated position stays within translationThreshold of the real one.
//
// Unlike the rotation there is no hemisphere test: a position can simply be
// interpolated linearly.  The distance is compared squared, so the tolerance
// is squared once up front.
//
// File layout of the translation of one bone:
//
//	short	number of keys, 0 if the bone never leaves the origin
//	then, only if there are keys on less frames than the animation has:
//		byte or short	frame number of every key (byte if numframes <= 256)
//	then float[3] for every key
//
// Two keys that are closer than the tolerance are merged into their average
// and written as a single key.

/*
=============
Animation::WriteTransFrames
=============
*/
void Animation::WriteTransFrames( float (*trans)[3] ) {
	float toleranceSq;
	float (*transKeys)[3];
	void *transIndices;
	bool bByteIndices;
	unsigned short numTransIndices;
	float pos[3];
	float frac;
	int frame, end, i, k;

	toleranceSq = model.translationThreshold * model.translationThreshold;

	bByteIndices = model.numframes <= 256;
	transIndices = alloca( bByteIndices ? model.numframes : model.numframes * 2 );
	transKeys = (float (*)[3])alloca( model.numframes * sizeof( float[3] ) );

	frame = 0;
	numTransIndices = 0;
	transKeys[0][0] = trans[0][0];
	transKeys[0][1] = trans[0][1];
	transKeys[0][2] = trans[0][2];
	if ( bByteIndices ) {
		( (unsigned char *)transIndices )[0] = 0;
	} else {
		( (unsigned short *)transIndices )[0] = 0;
	}

	while ( frame < model.numframes - 1 ) {
		for ( end = frame + 2; end < model.numframes; end++ ) {
			for ( k = frame + 1; k < end; k++ ) {
				frac = (float)( k - frame ) / (float)( end - frame );
				pos[0] = trans[frame][0] + ( trans[end][0] - trans[frame][0] ) * frac;
				pos[1] = trans[frame][1] + ( trans[end][1] - trans[frame][1] ) * frac;
				pos[2] = trans[frame][2] + ( trans[end][2] - trans[frame][2] ) * frac;
				if ( VectorDistanceSquared( pos, trans[k] ) >= toleranceSq ) {
					goto nextKey;
				}
			}
		}
		// end either failed interpolation or reached the frame count.
		// Keep the preceding frame as a key and start a new segment there.
		// Its frame number is stored separately for runtime lookup.
nextKey:

		numTransIndices++;
		frame = end - 1;
		transKeys[numTransIndices][0] = trans[frame][0];
		transKeys[numTransIndices][1] = trans[frame][1];
		transKeys[numTransIndices][2] = trans[frame][2];
		if ( bByteIndices ) {
			( (unsigned char *)transIndices )[numTransIndices] = (unsigned char)frame;
		} else {
			( (unsigned short *)transIndices )[numTransIndices] = (unsigned short)frame;
		}
	}
	for ( i = 0; i <= numTransIndices; i++ ) {
		if ( transKeys[i][0] * transKeys[i][0] + transKeys[i][1] * transKeys[i][1] + transKeys[i][2] * transKeys[i][2] >= toleranceSq ) {
			break;
		}
	}
	if ( i > numTransIndices ) {
		// the bone never moves
		numTransIndices = 0;
		SafeWrite( model.file, &numTransIndices, 2 );
		return;
	}
	if ( numTransIndices == 0 ) {
		numTransIndices = 1;
		SafeWrite( model.file, &numTransIndices, 2 );
		WriteVec3( transKeys[0] );
		return;
	}
	if ( numTransIndices == 1 && VectorDistanceSquared( transKeys[0], transKeys[1] ) < toleranceSq ) {
		SafeWrite( model.file, &numTransIndices, 2 );
		transKeys[0][0] = ( transKeys[0][0] + transKeys[1][0] ) * 0.5f;
		transKeys[0][1] = ( transKeys[0][1] + transKeys[1][1] ) * 0.5f;
		transKeys[0][2] = ( transKeys[0][2] + transKeys[1][2] ) * 0.5f;
		WriteVec3( transKeys[0] );
		return;
	}
	// more than one key: count, the frame numbers of the keys unless every
	// frame is a key, then the positions
	//
	// the count is an unsigned short, so numframes itself is limited to 65535
	numTransIndices++;
	SafeWrite( model.file, &numTransIndices, 2 );
	assert( numTransIndices <= model.numframes );
	if ( numTransIndices < model.numframes ) {
		if ( bByteIndices ) SafeWrite( model.file, transIndices, numTransIndices );
		else SafeWrite( model.file, transIndices, numTransIndices * 2 );
	}
	for ( i = 0; i < numTransIndices; i++ ) {
		WriteVec3( transKeys[i] );
	}
}
// Rotation around the z axis only: every frame is a (z, w) pair. 
// This is the cheap case for a bone that only turns about the vertical axis
// (doors, turrets, wheels).  Such a rotation is described by two numbers
// instead of four, so everything is done on pairs: the pairs are normalized
// and quantized like a full quaternion, but x and y are known to be zero.
// The reduction itself is the one of WriteQuatFrames; for the error measure
// the pair is widened to a real quaternion (0, 0, z, w) so that the same
// QuatRatioEigenTrace can be used.
//
// File layout of the rotation of one bone:
//
//	short	number of keys, 0 for "no rotation"
//	then, only if there are keys on less frames than the animation has:
//		byte or short	frame number of every key (byte if numframes <= 256)
//	then short z for every key (the sign is carried by w, which is dropped)
//
// The caller has already decided that the bone is z only.
/*
=============
Animation::WriteQuatFrames2D
=============
*/
void Animation::WriteQuatFrames2D( float (*_quatFrames)[2] ) {
	float tolerance;
	short (*_sQuatFrames)[2];
	short (*quatKeys)[2];
	short *key, *newKey;
	void *quatIndices;
	bool bByteIndices;
	unsigned short numQuatIndices;
	float quat2[2];
	float quatA[4], quatB[4];
	float frac;
	int frame, end, i, k;
	// Compare the quantized keys against the angular tolerance.
	tolerance = AngleEigenTrace( model.rotationThreshold );
	_sQuatFrames = (short (*)[2])alloca( model.numframes * sizeof( short[2] ) );
	for ( i = 0; i < model.numframes; i++ ) {
		QuatToShort2( _quatFrames[i], _sQuatFrames[i] );
	}
	assert( _quatFrames[0][1] >= 0 );
	assert( _sQuatFrames[0][1] >= 0 );

	for ( frame = 1; frame < model.numframes; frame++ ) {
		if ( _sQuatFrames[frame - 1][0] * _sQuatFrames[frame][0] + _sQuatFrames[frame - 1][1] * _sQuatFrames[frame][1] < 0 ) {
			_sQuatFrames[frame][0] = -_sQuatFrames[frame][0];
			_sQuatFrames[frame][1] = -_sQuatFrames[frame][1];
			float *q = _quatFrames[frame];
			q[0] = -q[0];
			q[1] = -q[1];
		}
	}

	// Starting at a key, find the farthest possible next key such that each frame in
	// between stays within tolerance after interpolation and normalization.
	//
	// The first frame is always a key.  Per key the frame number and the
	// quantized pair are kept:
	//	numQuatIndices	number of keys, not counting the first one
	//	quatKeys[]	the quantized key pairs
	//	quatIndices[]	their frame numbers, bytes if numframes <= 256
	//
	// A key is accepted while the frames in between stay within the tolerance
	// and the dot product with the key is positive.  A negative dot product
	// can not happen after the sign fixup above, but a pair that is
	// perpendicular to the key (dot product of exactly zero) can, and then
	// the interpolation would pass through the zero vector.  Stop there.
	//
	// Measuring the error:
	//	quatA	the interpolated pair, as (0, 0, z, w)
	//	quatB	the real pair of the frame in between, as (0, 0, z, w)
	//	QuatRatioEigenTrace( quatA, quatB ) is compared with the tolerance.
	//
	// Both keys and indices live on the stack; with the frame limits of the
	// format (65535) this is at most a few hundred kilobytes.
	bByteIndices = model.numframes <= 256;
	if ( bByteIndices ) quatIndices = alloca( model.numframes );
	else quatIndices = alloca( model.numframes * 2 );
	quatKeys = (short (*)[2])alloca( model.numframes * sizeof( short[2] ) );

	frame = 0;
	numQuatIndices = 0;
	quatKeys[0][0] = _sQuatFrames[0][0];
	quatKeys[0][1] = _sQuatFrames[0][1];
	if ( bByteIndices ) {
		( (unsigned char *)quatIndices )[0] = 0;
	} else {
		( (unsigned short *)quatIndices )[0] = 0;
	}

	// The first frame is already a key; reduce only if another frame exists.
	if ( frame < model.numframes - 1 ) {
		key = _sQuatFrames[0];
		do {
			for ( end = frame + 2; end < model.numframes; end++ ) {
				if ( _sQuatFrames[end][0] * key[0] + _sQuatFrames[end][1] * key[1] <= 0 ) {
					break;
				}
				for ( k = frame + 1; k < end; k++ ) {
					frac = (float)( k - frame ) / (float)( end - frame );
					quat2[0] = _quatFrames[frame][0] + ( _quatFrames[end][0] - _quatFrames[frame][0] ) * frac;
					quat2[1] = _quatFrames[frame][1] + ( _quatFrames[end][1] - _quatFrames[frame][1] ) * frac;
					VectorNormalize2D( quat2 );
					quatA[0] = 0;
					quatA[1] = 0;
					quatA[2] = quat2[0];
					quatA[3] = quat2[1];
					quatB[0] = 0;
					quatB[1] = 0;
					quatB[2] = _quatFrames[k][0];
					quatB[3] = _quatFrames[k][1];
					if ( QuatRatioEigenTrace( quatA, quatB ) >= tolerance ) {
						goto nextKey;
					}
				}
			}
	nextKey:
			numQuatIndices++;
			newKey = quatKeys[numQuatIndices];
			frame = end - 1;

			key = _sQuatFrames[frame];
			newKey[0] = key[0];
			newKey[1] = key[1];
			if ( bByteIndices ) {
				( (unsigned char *)quatIndices )[numQuatIndices] = (unsigned char)frame;
			} else {
				( (unsigned short *)quatIndices )[numQuatIndices] = (unsigned short)frame;
			}
		} while ( frame < model.numframes - 1 );
	}

	for ( i = 0; i <= numQuatIndices; i++ ) {
		if ( quatKeys[i][0] != 0 ) {
			break;
		}
	}
	if ( i > numQuatIndices ) {
		numQuatIndices = 0;
		SafeWrite( model.file, &numQuatIndices, 2 );
		return;
	}

	if ( numQuatIndices == 0 ) {
		numQuatIndices = 1;
		SafeWrite( model.file, &numQuatIndices, 2 );
		WriteQuatZ( quatKeys[0] );
		return;
	}

	if ( numQuatIndices == 1 && quatKeys[0][0] == quatKeys[1][0] && quatKeys[0][1] == quatKeys[1][1] ) {
		SafeWrite( model.file, &numQuatIndices, 2 );
		WriteQuatZ( quatKeys[0] );
		return;
	}

	// Every key is written as a single short.  A rotation around z is
	// (0, 0, sin(a/2), cos(a/2)); with w dropped and its sign folded
	// into z, one short per key is all the loader needs.
	//
	// The count includes the first key, which is not in numQuatIndices yet.
	// (A bone with exactly one key is handled above and never gets here.)
	numQuatIndices++;
	SafeWrite( model.file, &numQuatIndices, 2 );
	assert( numQuatIndices <= model.numframes );
	if ( numQuatIndices < model.numframes ) {
		if ( bByteIndices ) SafeWrite( model.file, quatIndices, numQuatIndices );
		else SafeWrite( model.file, quatIndices, numQuatIndices * 2 );
	}
	for ( i = 0; i < numQuatIndices; i++ ) {
		WriteQuatZ( quatKeys[i] );
	}
}

// Writes an XANIM file.
//
// The .gdf selects the animation type:
//	2	delta: one root's translation and z rotation relative to a node
//	1	relative: like absolute, but the root bone is skipped
//	0	absolute: every bone in the export
//
// File layout:
//
//	short	version (14)
//	short	number of frames
//	short	number of animated bones, not counting the skipped root
//	byte	flags: 1 = looping, 2 = delta
//	short	frame rate
//	delta only:
//		the z rotation of the root for every frame (WriteQuatFrames2D)
//		the translation of the root for every frame (WriteTransFrames)
//	if there are animated bones:
//		bit field, one bit per bone:  the first key of the bone points the
//			other way than the rotation of the bind pose
//		bit field, one bit per bone:  the rotation is around z only
//		the names of the bones, zero terminated
//		per bone the rotation (WriteQuatFrames) and the translation
//			(WriteTransFrames) of every frame
//	the notetracks (WriteNotetracks)
//
// The file omits the repeated last frame of a looping animation.
//
// baseFrames is the bind pose; rotations optionally overrides individual bones.
//

/*
=============
Animation::WriteAnimation
=============
*/
void Animation::WriteAnimation( const char *filename, int type, BoneFrame *baseFrames, const BoneRotation *rotations ) {
	char msg[2048];
	float q[4];
	// Inverse parent transform: rotation followed by translation.
	float matP[4][3];
	float matC[3][3];
	float negOffset[3];
	float baseOffset[3];
	short sQuat[4];
	float (*quatFrames)[4];
	float (*transFrames)[3];
	float (*zQuatFrames)[2];
	unsigned char *rotationBits;
	unsigned char *flagBits;
	const BoneRotation *rotation;
	BoneFrame *bf, *bfCurrent, *bfParent;
	Bone *boneName;
	unsigned char flags;
	bool bDelta, bSkipRoot, bFlag;
	int rootIndex, firstBone, numRoots, numAnimBones, numframes, numBitBytes, filePos, endPos;
	int parentIndex, bitIndex;
	int i, j, frame;
	short fileVersion, frameCount, boneCount, frameRate;

	bDelta = ( type == 2 );
	bSkipRoot = ( type == 2 || type == 1 );

	flags = 0;
	if ( model.looping ) {
		flags = 1;
	}
	if ( bDelta ) {
		flags |= 2;
	} else if ( model.nodePart != -1 ) {
		Error( "cannot specify node if type is not delta." );
	}

	Swap_Init();

	if ( model.looping && model.numframes != 1 ) {
		numframes = model.numframes - 1;
	} else {
		numframes = model.numframes;
	}
	if ( numframes == 0 ) {
		Error( "xanim '%s' has 0 frames", filename );
	}

	rootIndex = -1;
	if ( bSkipRoot ) {
		numRoots = 0;
		for ( i = 0; i < model.numbones; i++ ) {
			if ( model.bones[i].used ) {
				if ( model.bones[i].parent < 0 ) {
					rootIndex = i;
					numRoots++;
				} else if ( model.bones[i].stabilizer && model.bones[model.bones[i].parent].parent < 0 ) {
					rootIndex = i;
					numRoots++;
				}
			}
		}
		if ( bDelta && numRoots != 1 ) {
			sprintf( msg, "Not exactly one root bone for xanim '%s' with delta:\n", filename );
			for ( i = 0; i < model.numbones; i++ ) {
				Bone *const &bones = model.bones;
				if ( bones[i].used && bones[i].parent < 0 ) {
					strcat( msg, bones[i].name );
					strcat( msg, "\n" );
				}
			}
			Error( msg );
		}
		const int start = rootIndex + 1;
		firstBone = start;
	} else {
		firstBone = 0;
	}
	numAnimBones = 0;
	for ( i = firstBone; i < model.numbones; i++ ) {
		if ( model.bones[i].used ) {
			numAnimBones++;
		}
	}
	model.file = SafeOpenWrite( filename );
	fileVersion = LittleShort( 14 );
	SafeWrite( model.file, &fileVersion, 2 );
	assert( numframes == (short) numframes );
	assert( model.numframes == (short) model.numframes );
	frameCount = LittleShort( numframes );
	SafeWrite( model.file, &frameCount, 2 );

	assert( numAnimBones == (short) numAnimBones );
	boneCount = LittleShort( numAnimBones );
	SafeWrite( model.file, &boneCount, 2 );

	SafeWrite( model.file, &flags, 1 );

	assert( model.framerate == (short) model.framerate );
	frameRate = LittleShort( (unsigned short) model.framerate );
	SafeWrite( model.file, &frameRate, 2 );

	// per frame data of the bones, rebuilt for every bone
	quatFrames = (float (*)[4])alloca( model.numframes * sizeof( float[4] ) );
	transFrames = (float (*)[3])alloca( model.numframes * sizeof( float[3] ) );
	zQuatFrames = (float (*)[2])alloca( model.numframes * sizeof( float[2] ) );

	if ( bDelta ) {
		assert( bSkipRoot );
		assert( rootIndex >= 0 );

		// the root bone has to be animated
		// (a bone nobody references would not exist in the animation)
		boneName = &model.bones[rootIndex];
		if ( !boneName->used ) {
			fclose( model.file );
			Error( "Part '%s' was not animated.", boneName->name );
		}
		assert( boneName->parent == -1 );

		if ( model.nodePart == -1 ) {
			bf = &model.frames[0].bones[rootIndex];
			MatrixTranspose( bf->axis, matP );
			negOffset[0] = bf->offset[0] * -1;
			negOffset[1] = bf->offset[1] * -1;
			negOffset[2] = bf->offset[2] * -1;
			MatrixTransformVector( negOffset, matP, matP[3] );
		}

		// The root bone is reduced to a rotation around z and a translation,
		// both relative to the node: the inverse of the node transform is
		// applied to the root of every frame.  Without a node, the pose of
		// the root in the first frame is the origin of the movement.
		//
		// matP is that inverse transform: the transposed rotation of the
		// node and, in the fourth row, its translation.
		for ( frame = 0; frame < model.numframes; frame++ ) {
			if ( model.nodePart != -1 ) {
				bf = &model.nodeFrames[frame];
				MatrixTranspose( bf->axis, matP );
				negOffset[0] = bf->offset[0] * -1;
				negOffset[1] = bf->offset[1] * -1;
				negOffset[2] = bf->offset[2] * -1;
				MatrixTransformVector( negOffset, matP, matP[3] );
			}
			bfCurrent = &model.frames[frame].bones[rootIndex];
			MatrixMultiply( bfCurrent->axis, matP, matC );
			MatrixTransformVector43( bfCurrent->offset, matP, transFrames[frame] );
			MatrixToQuat( matC, q );
			if ( q[0] * q[0] + q[1] * q[1] >= ( q[2] * q[2] + q[3] * q[3] ) * 0.001f ) {
				fclose( model.file );
				Error( "xanim '%s' has bad angle delta on frame %d", filename, model.frames[frame].number );
			}
			zQuatFrames[frame][0] = q[2];
			zQuatFrames[frame][1] = q[3];
		}

			assert( model.numframes > 0 );
		if ( model.looping ) {
			zQuatFrames[model.numframes - 1][0] = zQuatFrames[0][0];
			zQuatFrames[model.numframes - 1][1] = zQuatFrames[0][1];
		}
		WriteQuatFrames2D( zQuatFrames );
		WriteTransFrames( transFrames );
	}

	if ( numAnimBones != 0 ) {
		// Two bit fields precede the bone data.  The bits are not known before
		// the bones were written, so the fields are written as zeroes first
		// and filled in at the end: the position is remembered and the
		// file is rewritten from there.  The names of all bones come next
		// so that the loader can resolve them before it reads any frame.
		//
		// The bytes are cleared by memset, as the stack has no defined content.
		numBitBytes = ( ( numAnimBones - 1 ) >> 3 ) + 1;
		rotationBits = (unsigned char *)alloca( numBitBytes );
		memset( rotationBits, 0, numBitBytes );
		flagBits = (unsigned char *)alloca( numBitBytes );
		memset( flagBits, 0, numBitBytes );

		filePos = ftell( model.file );
		SafeWrite( model.file, rotationBits, numBitBytes );
		SafeWrite( model.file, flagBits, numBitBytes );

		for ( j = firstBone; j < model.numbones; j++ ) {
			if ( model.bones[j].used ) {
				SafeWrite( model.file, model.bones[j].name, strlen( model.bones[j].name ) + 1 );
			}
		}

		bitIndex = 0;
		for ( j = firstBone; j < model.numbones; j++ ) {
			Bone *bone = &model.bones[j];
			if ( !bone->used ) {
				continue;
			}
			parentIndex = bone->parent;
			while ( parentIndex >= 0 && !model.bones[parentIndex].used ) {
				parentIndex = model.bones[parentIndex].parent;
			}

			for ( rotation = rotations; rotation; rotation++ ) {
				if ( !stricmp( bone->name, rotation->name.c_str() ) ) {
					break;
				}
				if ( !rotation->more ) {
					rotation = 0;
					break;
				}
			}
			if ( rotation ) {
				q[0] = rotation->quat[0];
				q[1] = rotation->quat[1];
				q[2] = rotation->quat[2];
				q[3] = rotation->quat[3];
			} else {
				int baseParent = bone->fileIndex;
				bfCurrent = &baseFrames[baseParent];
				if ( parentIndex >= 0 ) {
					baseParent = model.bones[parentIndex].fileIndex;
				}
				bfParent = &baseFrames[baseParent];
				MatrixTranspose( bfParent->axis, matP );
				MatrixMultiply( bfCurrent->axis, matP, matC );
				MatrixToQuat( matC, q );
			}

			for ( frame = 0; frame < model.numframes; frame++ ) {
				bfCurrent = &model.frames[frame].bones[j];
				if ( parentIndex >= 0 ) {
					// a parent always comes before its children
					assert( parentIndex < j );
					bfParent = &model.frames[frame].bones[parentIndex];
				} else {
					bfParent = &baseFrames[bone->fileIndex];
				}
				MatrixTranspose( bfParent->axis, matP );
				negOffset[0] = bfParent->offset[0] * -1;
				negOffset[1] = bfParent->offset[1] * -1;
				negOffset[2] = bfParent->offset[2] * -1;
				MatrixTransformVector( negOffset, matP, matP[3] );
				MatrixTransformVector43( bfCurrent->offset, matP, transFrames[frame] );
				MatrixMultiply( bfCurrent->axis, matP, matC );
				MatrixToQuat( matC, quatFrames[frame] );
			}

			// the last frame of a looping animation is a copy of the first one
			assert( model.numframes > 0 );
			if ( model.looping ) {
				quatFrames[model.numframes - 1][0] = quatFrames[0][0];
				quatFrames[model.numframes - 1][1] = quatFrames[0][1];
				quatFrames[model.numframes - 1][2] = quatFrames[0][2];
				quatFrames[model.numframes - 1][3] = quatFrames[0][3];
			}
			QuatToShort( quatFrames[0], sQuat );
			// Preserve the first key's sign relative to the bind pose.
			// WriteQuatFrames uses it when dropping the fourth component.
			bFlag = sQuat[3] * q[3] + sQuat[2] * q[2] + sQuat[1] * q[1] + sQuat[0] * q[0] < 0;
			if ( bFlag ) rotationBits[bitIndex >> 3] |= 1 << ( bitIndex & 7 );
			if ( model.useBones && parentIndex >= 0 ) {
				bfCurrent = &baseFrames[bone->fileIndex];
				bfParent = &baseFrames[model.bones[parentIndex].fileIndex];
				MatrixTranspose( bfParent->axis, matP );
				negOffset[0] = bfParent->offset[0] * -1;
				negOffset[1] = bfParent->offset[1] * -1;
				negOffset[2] = bfParent->offset[2] * -1;
				MatrixTransformVector( negOffset, matP, matP[3] );
				MatrixTransformVector43( bfCurrent->offset, matP, baseOffset );
				for ( frame = 0; frame < model.numframes; frame++ ) {
					transFrames[frame][0] -= baseOffset[0];
					transFrames[frame][1] -= baseOffset[1];
					transFrames[frame][2] -= baseOffset[2];
				}
			}
			assert( model.numframes > 0 );
			if ( model.looping ) {
				transFrames[model.numframes - 1][0] = transFrames[0][0];
				transFrames[model.numframes - 1][1] = transFrames[0][1];
				transFrames[model.numframes - 1][2] = transFrames[0][2];
			}

			if ( WriteQuatFrames( quatFrames, bFlag ) ) {
				flagBits[bitIndex >> 3] |= 1 << ( bitIndex & 7 );
			}
			WriteTransFrames( transFrames );
			bitIndex++;
		}

		endPos = ftell( model.file );
		fseek( model.file, filePos, SEEK_SET );
		SafeWrite( model.file, rotationBits, numBitBytes );
		SafeWrite( model.file, flagBits, numBitBytes );
		fseek( model.file, endPos, SEEK_SET );
	}

	WriteNotetracks();
	fclose( model.file );
}

/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Token reader for model and animation export files.

#ifndef SCRIPT_H
#define SCRIPT_H

#include <tchar.h>
#include <string>

class Script {
	bool tokenready;
	std::string filename;
	char *script_p;
	char *end_p;
	int line;
	char token[256];
	bool releaseBuffer;
	char *buffer;
	int length;
public:
	void Close();
	void Reset();
	bool SkipToEOL();
	bool CommentAvailable();
	void Error( const char *fmt, ... );
	void CheckOverflow();
	void SkipWhiteSpace( bool crossline );
	void SkipNonToken( bool crossline );
	char *GetString( bool crossline );
	~Script();
	Script();
	char *GetToken( bool crossline );
	void MatchToken( const char *match, bool crossline );
	int GetInteger( bool crossline );
	double GetDouble( bool crossline );
	float GetFloat( bool crossline );
	void GetVector( float *v, bool crossline );
	void Parse( char *data, int size, const char *name );
	void LoadFile( const char *name );
};



#endif

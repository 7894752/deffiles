/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Converter value tree supplied by the asset manager.

#ifndef CONVERT_VALUE_H
#define CONVERT_VALUE_H

// Values returned by ConvertValue::GetPrimType.
enum {
	CV_STRING,
	CV_INT,
	CV_FLOAT
};

class ConvertValue {
public:
	virtual const char *GetName() = 0;
	virtual bool GetVerbose() = 0;
	virtual const char *GetSourceDirectory() = 0;
	virtual const char *GetTargetDirectory( unsigned index ) = 0;
	virtual const char *GetRelativeTargetDirectory( unsigned index ) = 0;
	virtual const char *GetSourceRootDirectory() = 0;
	virtual const char *GetTargetRootDirectory() = 0;
	virtual const char *GetNextKey( const char *key ) = 0;
	virtual ConvertValue *GetValue( const char *key ) = 0;
	virtual bool IsPrimType() = 0;
	virtual const char *GetString() = 0;
	virtual int GetInteger() = 0;
	virtual float GetFloat() = 0;
	virtual void AddTargetFile( const char *path ) = 0;
	virtual void AddSourceFile( const char *path ) = 0;
	virtual int GetPrimType() = 0;
};

extern "C" __declspec( dllexport ) int Convert( ConvertValue *cv );

#endif

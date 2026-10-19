/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef SHADERLIB_H
#define SHADERLIB_H

class ConvertValue;

struct ShaderType {
	ShaderType *next;
	char name[0x400];
	char *text;
};

struct ShaderFile {
	ShaderFile *next;
	char path[0x400];
};

class ShaderLibrary {
public:
	ConvertValue *cv;
	char dir[0x40];
	ShaderType *types;
	char *text;
	ShaderFile *files;
	bool loaded;

	ShaderLibrary();
	~ShaderLibrary();
	char *GetShaderType( const char *name );
	char *LoadShaderFiles();
	char *FindShader( const char *name );
};

extern ShaderLibrary shaderLibrary;

#include "surfaceflags.h"

#endif

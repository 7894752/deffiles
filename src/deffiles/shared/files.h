/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef CONVERTER_FILES_H
#define CONVERTER_FILES_H
#include <stdio.h>
#include <stdlib.h>
extern "C" void Com_Error(int code, const char *fmt, ...);
/*
=============
FS_ReadFile
=============
*/
extern "C" inline int FS_ReadFile(const char *name, void **buffer) {
	FILE *file;
	int len;
	char *data;
	file = fopen(name, "rb");
	if (!file) return -1;
	fseek(file, 0, SEEK_END);
	len = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = (char *)malloc(len + 1);
	if (fread(data, 1, len, file) != len) Com_Error(0, "Short read in FS_ReadFile()\n");
	data[len] = 0;
	fclose(file);
	*buffer = data;
	return len;
}

/*
=============
FS_FreeFile
=============
*/
extern "C" inline void FS_FreeFile(void *buffer) {
	free(buffer);
}

#endif

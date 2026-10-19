/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Assertion reporting with symbolic stack traces from linker map files.

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "assertive.h"

class Line {
public:
	const char *file;
	unsigned address;
	int number;
	Line *next;

	/*
	=============
	Line::Line
	=============
	*/
	Line( Line *head ) {
		next = head;
	}

	/*
	=============
	Line::~Line
	=============
	*/
	~Line() {
		delete next;
	}
};

class Symbol {
public:
	const char *object;
	unsigned address;
	const char *name;
	Symbol *next;

	/*
	=============
	Symbol::Symbol
	=============
	*/
	Symbol( Symbol *head ) {
		next = head;
	}

	/*
	=============
	Symbol::~Symbol
	=============
	*/
	~Symbol() {
		delete next;
	}
};

class StringEntry {
public:
	char *string;
	unsigned hash;
	StringEntry *next;

	/*
	=============
	StringEntry::StringEntry
	=============
	*/
	StringEntry( const char *source, unsigned sourceHash, StringEntry *head ) {
		next = head;
		string = new char[strlen( source ) + 1];
		strcpy( string, source );
		hash = sourceHash;
	}

	/*
	=============
	StringEntry::~StringEntry
	=============
	*/
	~StringEntry() {
		delete next;
		delete string;
	}
};

class Module {
public:
	const char *name;
	unsigned base;
	unsigned end;
	Symbol *symbols;
	Line *lines;
	StringEntry *strings[256];
	Module *next;

	/*
	=============
	Module::Module
	=============
	*/
	Module() {
		name = 0;
		base = 0;
		end = 0;
		symbols = 0;
		lines = 0;
		next = 0;
		memset( strings, 0, sizeof( strings ) );
	}

	/*
	=============
	Module::~Module
	=============
	*/
	~Module() {
		int i;

		delete next;
		delete name;
		delete symbols;
		delete lines;
		for ( i = 0; i < 256; i++ ) {
			delete strings[i];
		}
	}
};

static HFONT hFont;

void ( *Assert_MessageHook )( const char *message );

static Module *modules;

/*
=============
HashString
=============
*/
static unsigned HashString( const char *string ) {
	unsigned hash;

	hash = 0;
	for ( ; *string; string++ ) {
		hash = hash * 31337 + *string;
	}
	return hash;
}

// returns the unique copy of string owned by the module
/*
=============
InternString
=============
*/
static const char *InternString( const char *string, Module *module ) {
	StringEntry *entry;
	unsigned hash, index;

	hash = HashString( string );
	index = hash & 255;
	for ( entry = module->strings[index]; entry; entry = entry->next ) {
		if ( entry->hash == hash && !strcmp( entry->string, string ) ) {
			return entry->string;
		}
	}

	entry = new StringEntry( string, hash, module->strings[index] );
	module->strings[index] = entry;
	return entry->string;
}

/*
=============
ParseError
=============
*/
static void ParseError( const char *message ) {
	MessageBoxA( 0, message, ".map parse error", MB_ICONERROR );
}

/*
=============
ReadLine
=============
*/
static bool ReadLine( char *buffer, int size, FILE *f, bool eofIsError ) {
	if ( fgets( buffer, size, f ) ) {
		return true;
	}
	if ( eofIsError ) {
		ParseError( "Unexpected end-of-file parsing .map file" );
	}
	return false;
}

/*
=============
SkipLines
=============
*/
static bool SkipLines( char *buffer, int size, int count, FILE *f, bool eofIsError ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		if ( !ReadLine( buffer, size, f, true ) ) {
			return false;
		}
	}
	return true;
}

/*
=============
ParseMapFile
=============
*/
static bool ParseMapFile( FILE *f, Module *module, unsigned base ) {
	char line[0x8000];
	char objectName[1024];
	char symbolName[1024];
	int preferredLoad;
	int segment, offset, length;
	int lineNumber[4], lineSegment[4], lineOffset[4];
	int count, i, k;
	const char *file;
	char *p, *end;

	line[sizeof( line ) - 1] = 0;

	do {
		if ( !ReadLine( line, sizeof( line ) - 1, f, true ) ) {
			return false;
		}
	} while ( sscanf( line, " Preferred load address is %x\r\n", &preferredLoad ) != 1 );
	module->base = base;

	if ( !SkipLines( line, sizeof( line ) - 1, 2, f, true ) ) {
		return false;
	}

	// segments
	if ( !ReadLine( line, sizeof( line ) - 1, f, true ) ) {
		return false;
	}
	do {
		if ( !strcmp( line, "\r\n" ) ) {
			goto publics;
		}
		if ( sscanf( line, "%x:%x %xH %s %s", &segment, &offset, &length, symbolName, objectName ) != 5 ) {
			ParseError( "Unknown line format in the segments section" );
			return false;
		}
		if ( segment == 1 && module->end < offset + length + base + 0x1000 ) {
			module->end = offset + length + base + 0x1000;
		}
	} while ( ReadLine( line, sizeof( line ) - 1, f, true ) );
	return false;

publics:

	// public symbols
	do {
		if ( !ReadLine( line, sizeof( line ) - 1, f, true ) ) {
			return false;
		}
	} while ( strcmp( line, "  Address         Publics by Value              Rva+Base     Lib:Object\r\n" ) );
	if ( !SkipLines( line, sizeof( line ) - 1, 1, f, true ) ) {
		return false;
	}
	if ( !ReadLine( line, sizeof( line ) - 1, f, true ) ) {
		return false;
	}
	do {
		if ( !strcmp( line, "\r\n" ) ) {
			goto statics;
		}
		if ( sscanf( line, "%x:%x %s %x", &segment, &offset, symbolName, &length ) != 4 ) {
			ParseError( "Unknown line format in the public symbols section" );
			return false;
		}
		p = strrchr( line, ' ' );
		if ( !p || sscanf( p + 1, "%s", objectName ) != 1 ) {
			ParseError( "Couldn't parse file name in the public symbols section" );
			return false;
		}
		module->symbols = new Symbol( module->symbols );
		module->symbols->object = InternString( objectName, module );
		module->symbols->address = length - preferredLoad + base;
		module->symbols->name = InternString( symbolName, module );
	} while ( ReadLine( line, sizeof( line ) - 1, f, true ) );
	return false;

statics:
	if ( !SkipLines( line, sizeof( line ) - 1, 2, f, true ) ) {
		return false;
	}
	if ( !ReadLine( line, sizeof( line ) - 1, f, true ) ) {
		return false;
	}
	if ( !strcmp( line, " Static symbols\r\n" ) ) {
		if ( !SkipLines( line, sizeof( line ) - 1, 1, f, true ) ) {
			return false;
		}
		while ( ReadLine( line, sizeof( line ) - 1, f, false ) ) {
			if ( !line[0] || !strcmp( line, "\r\n" ) ) {
				break;
			}
			if ( sscanf( line, "%x:%x %s %x", &segment, &offset, symbolName, &length ) != 4 ) {
				ParseError( "Unknown line format in the static symbols section" );
				return false;
			}
			p = strrchr( line, ' ' );
			if ( !p || sscanf( p + 1, "%s", objectName ) != 1 ) {
				ParseError( "Couldn't parse file name in the static symbols section" );
				return false;
			}
			module->symbols = new Symbol( module->symbols );
			module->symbols->object = InternString( objectName, module );
			module->symbols->address = length;
			module->symbols->name = InternString( symbolName, module );
		}
	}

	// line numbers
	while ( ReadLine( line, sizeof( line ) - 1, f, false ) ) {
		if ( strncmp( line, "Line numbers for ", 17 ) ) {
			ParseError( "Expected line number section" );
			return false;
		}
		p = strchr( line, '(' );
		if ( !p ) {
			ParseError( "Couldn't find '(' for the name of the source file in line number section" );
			return false;
		}
		end = strchr( p, ')' );
		if ( !end ) {
			ParseError( "Couldn't find ')' for the name of the source file in line number section" );
			return false;
		}
		strncpy( objectName, p + 1, end - p - 1 );
		objectName[end - p - 1] = 0;
		file = InternString( objectName, module );

		if ( !SkipLines( line, sizeof( line ) - 1, 1, f, true ) ) {
			return false;
		}
		while ( ReadLine( line, sizeof( line ) - 1, f, false ) ) {
			if ( !line[0] || !strcmp( line, "\r\n" ) ) {
				break;
			}
			count = sscanf( line, "%i %x:%x %i %x:%x %i %x:%x %i %x:%x\r\n",
				&lineNumber[0], &lineSegment[0], &lineOffset[0],
				&lineNumber[1], &lineSegment[1], &lineOffset[1],
				&lineNumber[2], &lineSegment[2], &lineOffset[2],
				&lineNumber[3], &lineSegment[3], &lineOffset[3] );
			if ( count % 3 != 0 || count / 3 <= 0 ) {
				ParseError( "unknown line format in the line number section" );
				return false;
			}
			if ( count > 0 ) {
				i = 0;
				k = 0;
				do {
					module->lines = new Line( module->lines );
					module->lines->file = file;
					module->lines->address = lineOffset[k] + base + 0x1000;
					module->lines->number = lineNumber[k];
					i += 3;
					k++;
				} while ( i < count );
			}
		}
	}
	return true;
}

/*
=============
GetModuleBase
=============
*/
static unsigned GetModuleBase( const char *mapName ) {
	char name[260];
	int length, i;
	HMODULE module;

	length = strlen( mapName );
	for ( i = length - 1; i >= 0; i-- ) {
		if ( mapName[i] == '.' || mapName[i] == '/' || mapName[i] == '\\' ) {
			if ( i >= 0 && mapName[i] == '.' ) {
				length = i;
			}
			break;
		}
	}
	memcpy( name, mapName, length );
	strcpy( name + length, ".exe" );
	module = GetModuleHandleA( name );
	if ( !module ) {
		strcpy( name + length, ".dll" );
		module = GetModuleHandleA( name );
	}
	return (unsigned)module;
}

/*
=============
LoadMapFilesForDir
=============
*/
static void LoadMapFilesForDir( const char *dir ) {
	char pattern[2048];
	char path[2048];
	char moduleName[2048];
	WIN32_FIND_DATA findData;
	HANDLE handle;
	FILE *f;
	Module *module, *m;
	unsigned base;
	int length;

	if ( *dir ) {
		sprintf( pattern, "%s/*.map", dir );
	} else {
		strcpy( pattern, "*.map" );
	}

	handle = FindFirstFileA( pattern, &findData );
	if ( handle == INVALID_HANDLE_VALUE ) {
		return;
	}

	do {
		base = GetModuleBase( findData.cFileName );
		if ( !base ) {
			continue;
		}

		strcpy( moduleName, findData.cFileName );
		length = strlen( moduleName );
		moduleName[length - 4] = 0;
		for ( m = modules; m; m = m->next ) {
			if ( !stricmp( m->name, moduleName ) ) {
				goto nextFile;
			}
		}

		if ( *dir ) {
			sprintf( path, "%s/%s", dir, findData.cFileName );
		} else {
			strcpy( path, findData.cFileName );
		}
		f = fopen( path, "rb" );
		if ( f ) {
			module = new Module;
			if ( ParseMapFile( f, module, base ) ) {
				module->name = InternString( moduleName, module );
				module->next = modules;
				modules = module;
			} else {
				delete module;
			}
			fclose( f );
		}

nextFile:
		;
	} while ( FindNextFileA( handle, &findData ) );

	FindClose( handle );
}

/*
=============
LoadMapFiles
=============
*/
static void LoadMapFiles() {
	LoadMapFilesForDir( "" );
}

// writes one stack frame description, returns the number of characters written
/*
=============
FormatStackFrame
=============
*/
static int FormatStackFrame( char *out, unsigned address, bool *stop ) {
	const char *moduleName, *functionName;
	Symbol *bestSymbol, *symbol;
	Line *bestLine, *line;
	Module *module;
	char name[1024];
	char *p, *file;
	int length;

	*stop = false;
	bestSymbol = 0;
	bestLine = 0;
	moduleName = "<unknown module>";

	for ( module = modules; module; module = module->next ) {
		if ( address >= module->base && address < module->end ) {
			moduleName = module->name;
			for ( symbol = module->symbols; symbol; symbol = symbol->next ) {
				if ( symbol->address <= address && ( !bestSymbol || bestSymbol->address < symbol->address ) ) {
					bestSymbol = symbol;
				}
			}
			for ( line = module->lines; line; line = line->next ) {
				if ( line->address <= address && ( !bestLine || bestLine->address < line->address ) ) {
					bestLine = line;
				}
			}
			break;
		}
	}

	length = sprintf( out, "%s:\t", moduleName );
	p = out + length;

	if ( bestSymbol ) {
		functionName = bestSymbol->name;
		if ( *functionName == '_' || *functionName == '?' ) {
			functionName++;
		}
		strcpy( name, functionName );
		file = strchr( name, '@' );
		if ( file ) {
			*file = 0;
		}
		functionName = name;
		if ( !strcmp( name, "WinMain" ) || !strcmp( name, "main" ) ) {
			*stop = true;
		}
	} else {
		functionName = "<unknown function>";
		*stop = true;
	}

	if ( bestLine ) {
		file = strrchr( bestLine->file, '\\' );
		file = file ? file + 1 : (char *)bestLine->file;
		length = sprintf( p, "%s\t\t...%s, line %i", functionName, file, bestLine->number );
	} else if ( bestSymbol ) {
		file = strrchr( bestSymbol->object, '\\' );
		file = file ? file + 1 : (char *)bestSymbol->object;
		length = sprintf( p, "%s\t\t...%s", functionName, file );
	} else {
		length = sprintf( p, "%s", functionName );
	}
	p += length;

	return ( p - out ) + sprintf( p, "\r\n" );
}

/*
=============
Assert_DoStackTrace
=============
*/
int Assert_DoStackTrace( char *buffer, int maxFrames, int skipFrames ) {
	unsigned *frame;
	unsigned address;
	char *out;
	bool stop;
	int i;

	LoadMapFiles();

	__asm mov frame, ebp

	out = buffer;
	stop = false;
	for ( i = 0; i < maxFrames + skipFrames; i++ ) {
		__asm {
			mov ecx, frame
			mov eax, [ecx]
			mov frame, eax
			mov eax, [ecx + 4]
			mov address, eax
		}
		if ( i >= skipFrames ) {
			out += FormatStackFrame( out, address - 5, &stop );
			if ( stop ) {
				break;
			}
		}
	}
	return out - buffer;
}

// Cache the default dialog font; callers may supply a different font.
/*
=============
Assert_SetDialogFont
=============
*/
void Assert_SetDialogFont( HWND dialog, HFONT font = hFont ) {
	if ( dialog == GetDesktopWindow() ) {
		return;
	}
	if ( !font ) {
		hFont = font = (HFONT)GetStockObject( DEFAULT_GUI_FONT );
	}
	for ( HWND child = GetWindow( dialog, GW_CHILD ); child; child = GetWindow( child, GW_HWNDNEXT ) ) {
		SendMessageA( child, WM_SETFONT, (WPARAM)font, TRUE );
	}
	InvalidateRect( dialog, 0, TRUE );
}

/*
=============
Assert_MyHandler
=============
*/
int Assert_MyHandler( const char *expression, const char *file, int line ) {
	char message[0x8000];
	char moduleName[260];
	HGLOBAL global;
	char *text;
	int length;

	if ( !GetModuleFileNameA( 0, moduleName, sizeof( moduleName ) ) ) {
		strcpy( moduleName, "<unknown application>" );
	}

	length = sprintf( message, "Expression:\r\n\t%s\r\n\r\nModule:\t%s\r\nFile:\t%s\r\nLine:\t%d\r\n\r\n",
		expression, moduleName, file, line );
	Assert_DoStackTrace( message + length, 32, 1 );

	if ( OpenClipboard( GetDesktopWindow() ) ) {
		EmptyClipboard();
		global = GlobalAlloc( GMEM_MOVEABLE, strlen( message ) + 1 );
		if ( global ) {
			text = (char *)GlobalLock( global );
			if ( text ) {
				strcpy( text, message );
				GlobalUnlock( global );
				SetClipboardData( CF_TEXT, global );
			}
		}
		CloseClipboard();
	}

	if ( Assert_MessageHook ) {
		Assert_MessageHook( message );
	}

	MessageBoxA( 0, message, "ASSERTION FAILURE... (this text is on the clipboard)", MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND );
	ExitProcess( -1 );
	__asm int 3
}

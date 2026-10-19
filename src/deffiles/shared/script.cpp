/*
===========================================================================
Call of Duty converter plugins (2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// Token-file reader used by the model and animation exporters.
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern "C" void Error(const char *fmt, ...);
extern "C" void Com_Error(int code, const char *fmt, ...);

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
    void Error(const char *fmt, ...);
    void CheckOverflow();
    void SkipWhiteSpace(bool crossline);
    void SkipNonToken(bool crossline);
    char *GetLine(bool crossline);
    char *GetToken(bool crossline);
    char *GetString(bool crossline);
    ~Script();
    Script();
    void MatchToken(const char *match, bool crossline);
    int GetInteger(bool crossline);
    double GetDouble(bool crossline);
    float GetFloat(bool crossline);
    void GetVector(float *v, bool crossline);
    void Parse(char *data, int size, const char *name);
    void LoadFile(const char *name);
};

/*
=============
Script::Close
=============
*/
void Script::Close() {
    if (releaseBuffer && buffer) delete [] buffer;
    buffer = NULL;
    script_p = NULL;
    end_p = NULL;
    line = 0;
    releaseBuffer = false;
    tokenready = false;
    memset(token, 0, sizeof(token));
}

/*
=============
Script::Reset
=============
*/
void Script::Reset() {
    script_p = buffer;
    line = 1;
    tokenready = false;
}

/*
=============
Script::SkipToEOL
=============
*/
bool Script::SkipToEOL() {
    if (script_p >= end_p) return true;
    while (*script_p != '\n') {
        if (script_p >= end_p) return true;
        script_p++;
    }
    return false;
}

/*
=============
Script::CommentAvailable
=============
*/
bool Script::CommentAvailable() {
    char *p = script_p;
    if (p < end_p) {
        char c = *p;
        if (c == ';' || (++p < end_p && c == '/' && *p == '/')) return true;
    }
    return false;
}

/*
=============
Script::~Script
=============
*/
Script::~Script() { Close(); }

/*
=============
Script::CheckOverflow
=============
*/
void Script::CheckOverflow() {
    if (script_p >= end_p) Error("End of token file reached prematurely");
}

/*
=============
Script::SkipWhiteSpace
=============
*/
void Script::SkipWhiteSpace(bool crossline) {
    CheckOverflow();
    while (*script_p <= ' ' || *script_p == ',') {
        if (*script_p++ == '\n') {
            if (!crossline) Error("line is incomplete\n");
            line++;
        }
        CheckOverflow();
    }
}

/*
=============
Script::SkipNonToken
=============
*/
void Script::SkipNonToken(bool crossline) {
    SkipWhiteSpace(crossline);
    while (CommentAvailable()) {
        SkipToEOL();
        SkipWhiteSpace(crossline);
    }
}

// Reads the rest of the current line as one token.
/*
=============
Script::GetLine
=============
*/
char *Script::GetLine(bool crossline) {
    char *start;
    int len;
    if (tokenready) {
        tokenready = false;
        return token;
    }
    SkipNonToken(crossline);
    start = script_p;
    SkipToEOL();
    len = script_p - start;
    if (len < (int)sizeof(token) - 1) {
        memcpy(token, start, len);
        token[len] = 0;
        return token;
    }
    Error("Token too large");
    return token;
}

/*
=============
Script::Script
=============
*/
Script::Script() {
    buffer = NULL;
    script_p = NULL;
    end_p = NULL;
    line = 0;
    releaseBuffer = false;
    tokenready = false;
    memset(token, 0, sizeof(token));
}

/*
=============
Script::GetToken
=============
*/
char *Script::GetToken(bool crossline) {
    char *token_p;
    if (tokenready) {
        tokenready = false;
        return token;
    }
    SkipNonToken(crossline);
    if (*script_p == '"') return GetString(crossline);
    token_p = token;
    while (*script_p > ' ' && *script_p != ',' && !CommentAvailable()) {
        if (*script_p == '\\' && script_p < end_p - 1) {
            script_p++;
            switch (*script_p) {
            case 'n': case 'r': *token_p++ = '\n'; break;
            case '\'': *token_p++ = '\''; break;
            case '"': *token_p++ = '"'; break;
            case '\\': *token_p++ = '\\'; break;
            default: *token_p++ = *script_p; break;
            }
            script_p++;
        } else {
            *token_p++ = *script_p++;
        }
        if (token_p == token + sizeof(token)) Error("Token too large");
        if (script_p == end_p) break;
    }
    *token_p = 0;
    return token;
}

/*
=============
Script::GetString
=============
*/
char *Script::GetString(bool crossline) {
    char *token_p;
    if (tokenready) {
        tokenready = false;
        return token;
    }
    SkipNonToken(crossline);
    if (*script_p != '"') Error("Expecting string");
    script_p++;
    token_p = token;
    while (*script_p != '"') {
        if (*script_p == '\n') Error("Line is incomplete while reading string");
        if (*script_p == '\\' && script_p < end_p - 1) {
            script_p++;
            switch (*script_p) {
            case 'n': case 'r': *token_p++ = '\n'; break;
            case '\'': *token_p++ = '\''; break;
            case '"': *token_p++ = '"'; break;
            case '\\': *token_p++ = '\\'; break;
            default: *token_p++ = *script_p; break;
            }
            script_p++;
        } else {
            *token_p++ = *script_p++;
        }
        if (script_p >= end_p) Error("End of token file reached prematurely while reading string");
        if (token_p == token + sizeof(token)) Error("String too large");
    }
    *token_p = 0;
    script_p++;
    return token;
}

/*
=============
Script::Error
=============
*/
void Script::Error(const char *fmt, ...) {
    char text[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(text, fmt, args);
    va_end(args);
    ::Error("%s(%d): %s\n", filename.c_str(), line, text);
}

/*
=============
Script::MatchToken
=============
*/
void Script::MatchToken(const char *match, bool crossline) {
    GetToken(crossline);
    if (strcmp(token, match)) Error("Expecting '%s'", match);
}

/*
=============
Script::GetInteger
=============
*/
int Script::GetInteger(bool crossline) {
    GetToken(crossline);
    return atoi(token);
}

/*
=============
Script::GetDouble
=============
*/
double Script::GetDouble(bool crossline) {
    GetToken(crossline);
    return atof(token);
}

/*
=============
Script::GetFloat
=============
*/
float Script::GetFloat(bool crossline) {
    return (float)GetDouble(crossline);
}

/*
=============
Script::GetVector
=============
*/
void Script::GetVector(float *v, bool crossline) {
    v[0] = GetFloat(crossline);
    v[1] = GetFloat(crossline);
    v[2] = GetFloat(crossline);
}

/*
=============
Script::Parse
=============
*/
void Script::Parse(char *data, int size, const char *name) {
    Close();
    buffer = data;
    Reset();
    length = size;
    end_p = script_p + size;
    filename = name;
}

/*
=============
Script::LoadFile
=============
*/
void Script::LoadFile(const char *name) {
    FILE *file;
    int size;
    char *data;
    Close();
    file = fopen(name, "rt");
    if (!file) ::Error("Script::LoadFile :: Couldn't load %s\n", name);
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = new char[size];
    fread(data, 1, size, file);
    fclose(file);
    Parse(data, size, name);
    releaseBuffer = true;
}


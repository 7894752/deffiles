# Third-party notices

The reconstruction as a whole is distributed under `COPYING` (GPL-3.0-or-later with id Software's additional terms).
The components below are included under their own terms.

## id Software (Quake III Arena, Return to Castle Wolfenstein, Wolfenstein: Enemy Territory)

The shared library files in `src/common`, `src/universal` and `src/deffiles/shared` (math, string and parsing helpers, command
utilities) descend from code published by id Software. Those files keep their original headers ("Copyright (C) 1999-2005 Id Software,
Inc.", GNU GPL version 2 or, at your option, any later version) and are used here under version 3. The id headers must not be removed.
Some functions are close to the Return to Castle Wolfenstein / Enemy Territory versions; for that reason the additional terms that id
Software applied to those releases are part of `COPYING`. These reconstructed and adapted files are not an original id Software or
Infinity Ward source release.

## NvTriStrip (`src/libs/nvtristrip`)

NvTriStrip is a triangle stripifier published by NVIDIA (authors named in NVIDIA's distribution: Curtis Beeson, Joe Demers,
Jason Regier, Jon Stone). It was published freely, but none of its files, and no README of the library, contains a
license text. The sources in this repository are a reconstruction of an older revision of the library (not the version on
GitHub today), kept with their original attribution because the byte-exact result depends on that revision. If NVIDIA or
the rights holder objects, these five files will be removed; the build would then need a user-supplied copy of the library.

## Not included

No Microsoft software (compiler, linker, libraries, Platform SDK) and no part of the original DLLs is distributed here. Each
`build/sources/<plugin>.json` records the original's link timestamp and SHA-256, which are constants, not code.

# deffiles — Call of Duty Converter Plugins (Byte-Identical Source)

Compiles with Visual C++ 7.0 to the exact bytes of the eight converter DLLs that shipped with the 2003 Call of Duty mod tools.

The converter plugins turn the asset manager's entries (`.gdf` data) into game files. The source is written so that Microsoft
Visual C++ 7.0 (Visual Studio .NET 2002) reproduces the original machine code.

This project is not affiliated with or endorsed by Activision, Infinity Ward, id Software, ZeniMax, NVIDIA or Microsoft. "Call of
Duty", "Quake" and "Wolfenstein" are trademarks of their respective owners and are used here only to describe what the code is. No
game data, no original binary and no Microsoft software is part of this repository.

## The plugins

| DLL | What it does |
|---|---|
| `xmodel.dll` | Builds models from `XMODEL_EXPORT` files: the xmodel with its LODs, the bone parts and the render surfaces (triangle strips), collision surfaces and skin overrides. |
| `xanim.dll` | Builds animations from `XANIM_EXPORT` files: bone parts, compressed frames, notetracks, multiplayer bone stabilizers. |
| `character.dll` | Writes a character script (`.gsc`) that sets the body and head models (directly or from a model alias) and adds the hat, view model and attachments. |
| `aitype.dll` | Writes an AI type script (`.gsc`) with its level-editor spawner definition, weapons and character choices. |
| `mptype.dll` | Writes a multiplayer type script (`.gsc`) that picks one of its characters at random and precaches them all. |
| `xmodelalias.dll` | Writes a model alias script (`.gsc`) that returns the list of models to choose from. |
| `configstringconvert.dll` | Writes an entry as a configstring file (`\key\value` pairs), as used for weapons, info files, player models and vehicles. |
| `defaultconvert.dll` | The converter for entries without their own plugin: writes the entry's keys and values to a text file. |

## Status

| DLL | Project functions | Result |
|---|---:|---|
| defaultconvert.dll | 3 | byte-identical |
| configstringconvert.dll | 3 | byte-identical |
| xmodelalias.dll | 29 | byte-identical |
| mptype.dll | 30 | byte-identical |
| aitype.dll | 30 | byte-identical |
| character.dll | 28 | byte-identical |
| xanim.dll | 112 | byte-identical |
| xmodel.dll | 158 | byte-identical |

- Every project function compiles to the same bytes and relocations as the original, checked one by one (393 in total).
- Each DLL links normally. `--identical` writes a separate copy with the recorded link timestamp restored (the COFF header field and
  its copy in the export directory). Those copies are byte-identical to the originals; the SHA-256 of each is printed next to the
  original's. Code, data, imports, relocations, layout and the Rich header come from compilation and linking.
- Byte identity proves the compiled image, not the source text: where several spellings compile to the same bytes, the choice among
  them is not evidence about the original. See [docs/RECONSTRUCTION_NOTES.md](docs/RECONSTRUCTION_NOTES.md).

Details: [docs/KNOWN_DIFFERENCES.md](docs/KNOWN_DIFFERENCES.md).

## Requirements

- Windows and Python 3 (standard library only).
- Visual C++ 7.0, `cl.exe` 13.00.9466, with its Platform SDK and static CRT: either your own copy, or let the build download it
  (`--fetch-toolchain`, needs 7-Zip; see [build/README.md](build/README.md)). Other compilers do not give the same code.

## Build

```bat
python -I build\build.py --module all --fetch-toolchain --strict --identical
```

or, with an installed Visual C++ 7.0:

```bat
python -I build\build.py --module all --vc7 "C:\Program Files\Microsoft Visual Studio .NET\Vc7" --strict --identical
```

The build compiles each DLL's translation units, links it, and prints the SHA-256 of the result next to the recorded release hash.
It never runs or loads what it builds. Add `--original-dir C:\path\to\deffiles` to get a byte and section comparison with your own
copies if a result differs. All options are in [build/README.md](build/README.md).

## Binaries

This repository contains no DLLs. If you just need the converters, use the official Call of Duty mod tools.

## How the match is established

Each function is compiled with the original compiler and measured flags. Its instruction bytes, relocation targets, jump tables and
C++ exception-handling contributions must agree with the original. The complete linked image is then compared byte for byte,
including headers, data, imports, relocations and padding. The per-function checks used private tooling; the standalone build needs
only the source in `src/` and the manifests in `build/sources/`.

## Repository layout

```
src/        the reconstruction (deffiles/<plugin>/, deffiles/shared/, common/, universal/, q3map/, libs/nvtristrip/)
build/      build.py, build.bat, sources/<plugin>.json, fetch_toolchain.py, toolchain_manifest.json, README.md
docs/       known differences, notes on what is proven and what is a choice
COPYING     GPLv3 with id Software's additional terms
THIRD_PARTY_NOTICES.md
```

## License

GPL-3.0-or-later with the additional terms id Software applied to its Wolfenstein source releases: see [COPYING](COPYING). Files that
descend from the Quake III Arena source keep their original headers (GPL version 2 or, at your option, any later version), which is used
here under version 3. NVIDIA's NvTriStrip is included with its attribution; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Credits

- Reconstruction by Jane.
- Published by Rose.
- Thanks to Prawy (https://github.com/PrawyCoD1) for the idea for this project.

Evidence and sources:

- id Software: Quake III Arena and the Return to Castle Wolfenstein and Wolfenstein: Enemy Territory source releases, the base of
  the shared math, string, parsing and command libraries.
- NVIDIA: NvTriStrip (Curtis Beeson, Joe Demers, Jason Regier, Jon Stone), here in the early revision preserved in the Torque Game
  Engine source.
- The later releases of the same converter plugins in the Call of Duty 2, World at War and Black Ops mod tools, which show the
  later state of the same files.
- Function and variable names come from strings in the binaries, the published sources above and the symbol tables and link maps of
  other retail builds of the same game family, wherever the compiled image agrees; the rest are descriptive.

## Ports

This repository is the byte-identical reference build and stays that way. Ports to x64, to modern compilers or to other
platforms are welcome as forks; the hope is that someone takes this on.

# Reconstruction notes: what is proven and what is a choice

The DLLs are reproduced byte for byte. That proves the compiled result, not the exact text of the original source. These notes say
which parts of the source follow from evidence and which are choices among equivalent spellings.

## 1. Evidence order for names

1. Strings in the DLLs themselves (messages, assertion expressions, source file names).
2. Published studio sources of the same engine family (Quake III Arena, Return to Castle Wolfenstein, Wolfenstein: Enemy Territory).
3. Later releases of the same plugins (the Call of Duty 2, World at War and Black Ops mod tools), symbol tables of other builds of the
   game family, and the converter interface as later published tools describe it.
4. Otherwise a descriptive name in the style of the surrounding code. Such names are inferred, not recovered.

## 2. One source tree, several plugins, several dates

The plugins were built from one tree between July and October 2003, and several files are shared:

- `deffiles/shared/`: the assertion handler, command utilities, the token reader (`Script`), the universal math and string library.
- `buildmds.cpp` and `animation.cpp` serve both the model and the animation converter. The assertion line numbers in both DLLs fit one
  file per name. Each plugin compiles the parts it needs; `xmodel.dll` (October) was built from a later revision of `buildmds.cpp`
  than `xanim.dll` (September), with one extra function before the bone loader.
- Where two plugins were built from different revisions, each has its own copy (`deffiles/xanim`, `deffiles/xmodel`). This applies to
  `buildmds.cpp`, `animation.cpp`, `q_parse.c` and `tangentspace.cpp` (`deffiles/shared/` holds the September revision used by
  `xanim.dll`, `deffiles/xmodel/` the October revision used by `xmodel.dll`).

## 3. Code that the linker discards

`/OPT:REF` removes functions nothing calls, and `/GF` string literals go with them. They still leave measurable traces: the size of the
relocation section counts their absolute references, constants they define first keep their position, and the linker's folding of
identical functions depends on the whole set of compiled functions. The following unused code is present because those traces require
it. None of it is called by the DLL it is compiled into.

| Code | Where | Evidence |
|---|---|---|
| Tangent-space calculation | `deffiles/shared/tangentspace.cpp` (xanim), `deffiles/xmodel/tangentspace.cpp` (xmodel) | Its constants (0.1f, a double 0.0) sit exactly where the originals have them, between the objects that precede and follow it in link order. The Call of Duty 2 tools still use the file (`universal\tangentspace.cpp`) from the same position; its structure and parameter names follow that later revision. The two DLLs need two revisions: the October one clamps the arc-cosine input and has a separate tangent orthogonalization step, which places 0.1f before 0.0 as in `xmodel.dll`; the September one keeps `xanim.dll` exact. |
| The model-building half of `buildmds.cpp` (vertex, face, surface and bounds functions of `SkelModel`) | compiled into xanim | Supplies the relocation count and the identical-function folding order of `xanim.dll`; the file is shared with `xmodel.dll` (section 2). |
| The animation-loading half of `buildmds.cpp` (`Animation::Init`, `Free`, `LoadBones`, `FindFrame`, `LoadBasePose`, `LoadModel`, `ReadModel`) and its declarations in `deffiles/xmodel/animationdata.h` | compiled into xmodel | The mirror image of the row above: it places the `VERSION`, `MODEL` and `radialNormals` strings at their original addresses and supplies part of the relocation count of `xmodel.dll`. |
| `Script::GetLine` | `deffiles/shared/script.cpp` | The position of its "Token too large" literal among the reader's other strings; the method exists in later releases of the same token reader. |
| Parse-mark checks in `Com_ParseSetMark` / `Com_ParseReturnToMark` | both copies of `q_parse.c` | The same checks are live in the 2003 level editor built from the same library. |
| The assertion dialog's font helper | `deffiles/shared/assertive.cpp` | The GDI32 import and the data layout of the small plugins. |
| The index remapper `RemapTriangleListIndices` | `q3map/tristrip_nv.cpp` (xmodel) | The strip adapter is shared with the map compiler; the converter does not remap. Its use of the library's `PrimitiveGroup` copies contributes to the folding order of the library's template functions. |

The exact bodies of discarded code cannot be checked byte for byte: only its traces can.

## 4. Things written to match the compiler

- Assertions use the original handler (`Assert_MyHandler`) with a breakpoint; the line numbers come from the original messages.
- Function order inside a file follows the original address order, which is the order the compiler emits them.
- NvTriStrip (`libs/nvtristrip`) follows the early public copy of the library preserved in the Torque Game Engine source. Where that copy
  leaves a choice open, the choice was made by the complete image: `PrimitiveGroup` has an ordinary out-of-line default constructor and
  an owning copy constructor (it deletes its index array, so a copy must duplicate it), and a few library functions are defined in a
  different order. These change which copies of the library's container templates the linker keeps, not any function's code.
- `NvTriStripObjects.cpp` and `NvTriStrip.cpp` use the compiler's automatic precompiled header (`/YX`), as the library's own project did.
  A precompiled header left over from an earlier build changes the order in which templates are emitted, so `build.py` always removes
  it before compiling; every build then starts from the same state the original did.

## 5. Line markers in the model and animation converters

The converters' assertion handler records each assertion's source line number, so the DLLs fix the exact line every assertion
was on. In `deffiles/xanim/buildmds.cpp` and in the model converter's `buildmds.cpp`, `animation.cpp` and `xmodelconverter.cpp`
(54 markers in all), those numbers show
that the original files were longer than the code that could be recovered (for example, an assertion on line 2142 of a file
whose reconstructed code ends well before that). The missing text was most likely comments, which leave no trace in the
binary. Rather than invent text to fill the gap, these files use `#line` directives that place the following code at its
original line number. They change no code; removing them changes only the line numbers inside the assertion messages.

## 6. Inferred names

The plugin entry points (`Convert`, `DllMain`), the converter interface methods, messages and source file names are recovered from the
DLLs and later releases. Class, structure, field and local names without such evidence are descriptive.

## 7. Limits of identity

Identity covers the eight DLLs as linked from these sources with the documented compiler and flags. It does not prove the original
file layout, comments, or the text of code the linker discards.

## Credits

- Reconstruction by Jane.
- Published by Rose.
- Thanks to Prawy (https://github.com/PrawyCoD1) for the idea for this project.

# Building the converter DLLs

A standalone build of the eight converter plugins from the 2003 Call of Duty mod tools. The scripts use Python 3's standard library
and Visual C++ 7.0 (`cl.exe` 13.00.9466, `link.exe` 7.00.9466). No Python packages need installing. The build never executes or loads
its output. The original DLLs and Microsoft toolchain binaries are not distributed with the source.

## Requirements

| What | Version |
|---|---|
| Compiler | `cl.exe` **13.00.9466** (Visual C++ 7.0, Visual Studio .NET 2002) |
| Linker / librarian | `link.exe` / `lib.exe` **7.00.9466** |
| CRT and C++ library | `libcmt.lib`, `libcpmt.lib` and `oldnames.lib` from that installation (`/MT`) |
| Platform SDK | August 2001 headers and import libraries: `kernel32`, `user32`, `gdi32` |
| Python | Python 3; standard library only |
| OS | Windows; the compiler and linker are 32-bit Windows programs, and the build uses `subst` (see below) |

Use your own Visual C++ 7.0 installation or `--fetch-toolchain`. The build checks selected tool/library SHA-256 values and warns on a
mismatch; `--strict` makes a mismatch an error. Other compiler builds do not reproduce the same machine code.

## Usage

Run from the repository root:

```
python -I build/build.py --module all --fetch-toolchain --strict --identical
```

| Option | Meaning |
|---|---|
| `--module NAME` | `all` (default) or one of defaultconvert, configstringconvert, xmodelalias, mptype, aitype, character, xanim, xmodel |
| `--vc7 DIR` | the `Vc7` folder of a Visual C++ 7.0 installation (default `build/toolchain/vc7`) |
| `--fetch-toolchain` | run `fetch_toolchain.py` first when no compiler is found at `--vc7` |
| `--identical` | also write `<module>_identical.dll` with the original link timestamp |
| `--original-dir DIR` | folder with your original DLLs; prints a byte and section comparison when a result differs |
| `--strict` | treat a toolchain hash mismatch as an error |
| `--out DIR` | output folder (default `build/out/<module>`) |

Each module prints `<module> sha256` and `<module> original`, then `IDENTICAL` or `differs`; `--module all` ends with a summary.

### Getting the toolchain automatically

`fetch_toolchain.py` downloads the files named in `toolchain_manifest.json` from public Internet Archive images of the VS .NET 2002
discs and the August 2001 Platform SDK. It verifies every hash and extracts only those files into `build/toolchain/`. It needs 7-Zip
and a network connection; it runs nothing it downloads. You are responsible for your own licence for those Microsoft files.

```
python -I build/fetch_toolchain.py --7z "C:\Program Files\7-Zip\7z.exe"
python -I build/fetch_toolchain.py --verify-only
python -I build/build.py --module all --identical --strict
```

## Source file names and the temporary drive

The compiler records the name it was given for `__FILE__`, and the DLLs contain those names in their assertion messages. The original
project passed each file by its bare name from its own directory, so the build does the same. One shared library file was compiled
by a rooted path without a drive letter (`\trees\mksrc\src\universal\q_shared.c`). To reproduce that, the build copies the file
under `build/root`, maps the first free drive letter (from Z: down) to that folder with `subst`, compiles from there, and removes the
mapping at the end. The letter never appears in the output. `build.py` only removes a mapping it created itself.

## Output

`build/out/<module>/` holds the objects, the linked `<module>.dll`, the map file, the linker response file and logs, and with
`--identical` the stamped `<module>_identical.dll`. The stamp writes only the link timestamp (the COFF header field and the export
directory's copy); the DLLs have no debug directory.

# Known differences from the original converter DLLs

Originals: the eight converter plugins of the Call of Duty (2003) mod tools, built between July and October 2003 with Visual C++ 7.0
(`cl.exe` 13.00.9466, `link.exe` 7.00.9466).

| DLL | Built (link timestamp, UTC) | Size | SHA-256 | Result |
|---|---|---:|---|---|
| defaultconvert.dll | 2003-07-10 18:13:52 | 53,248 | `036ef3220d833f68e2007e7c49cf56a82538963e7314c0282743f7ea4bf9eaa9` | identical |
| configstringconvert.dll | 2003-07-10 18:13:25 | 49,152 | `ea35e7cad99a5c0c33ffa4e1ab0ca7929ea51a06e12702738aa61921f030e830` | identical |
| xmodelalias.dll | 2003-07-10 18:14:38 | 73,728 | `f49ada71d86c53f900beaa16660759151cec2506e07bd888947f727ad122d690` | identical |
| mptype.dll | 2003-07-18 21:25:02 | 81,920 | `cd7aa4bfa6bcad2bfdec7f2025cc3fc80521890878af8b8446f714140adc7265` | identical |
| aitype.dll | 2003-09-08 04:42:08 | 81,920 | `74e932664a828850fe675c070f5075cf847c86c79c41dcc6fb4722a16eaa0b60` | identical |
| character.dll | 2003-09-05 06:08:51 | 77,824 | `598485c0d46219c5a8584996996c52975ce3ddb84768a957c7452d86367a84ac` | identical |
| xanim.dll | 2003-09-24 18:47:56 | 172,032 | `42e50e7951c505ce3c037b7990642a4bd8ff07fc3499515b42bb4ecebc41321e` | identical |
| xmodel.dll | 2003-10-25 00:01:20 | 262,144 | `b06fac3294b43c3abec172c69018ceac033d5d43db8d7d80add58cb92afcfe6e` | identical |

## What the build reproduces

- All 393 project functions compile to the original bytes and relocations (checked function by function), including the C++
  exception-handling tables. CRT and C++ library code comes from your Visual C++ 7.0 installation and is not part of this repository.
- Each DLL is compared section by section with its original: headers, `.text`, `.rdata`, `.data`, the import and export tables, the
  base relocations (including the size of the relocation section) and the Rich header, which the compile and link produce themselves.

## Metadata normalized after linking

| Difference | Where | Cause |
|---|---|---|
| Link timestamp | COFF header and the export directory's copy | `link.exe` 7.00.9466 stamps the time of the link and has no option to fix it. |

`build.py --identical` writes the recorded value into a copy (`<module>_identical.dll`); the ordinary linked DLL is kept. The DLLs have
no debug directory, so there is no PDB GUID or age.

## Byte identity is not source identity

Byte identity proves the compiled image, not the source text. Where several spellings compile to the same bytes, the choice among them
is not evidence about the original. See [Reconstruction notes](RECONSTRUCTION_NOTES.md).

## How to check

```text
python -I build/build.py --module all --vc7 <Vc7> --strict --identical --original-dir <folder with the original DLLs>
```

The command compiles and links every DLL, writes the stamped copies and compares each SHA-256 with the recorded original hash. If a hash
differs, `--original-dir` adds a per-section comparison and a whole-file byte count. No image is executed.

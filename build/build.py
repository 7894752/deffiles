#!/usr/bin/env python3
"""Build the Call of Duty (2003) converter plugin DLLs with Visual C++ 7.0 and compare them with the originals.

Uses Python 3's standard library, the sources in src/ and the manifests in build/sources/<module>.json.
The build compiles and links; it never executes or loads the resulting images.

Examples (run from the repository root):
    python -I build/build.py --module all --fetch-toolchain --strict --identical
    python -I build/build.py --module xanim --vc7 "C:\\Program Files\\Microsoft Visual Studio .NET\\Vc7" --identical

--identical also writes <module>_identical.dll with the original link timestamp restored (the only field the
linker cannot reproduce). The printed SHA-256 is compared with the original's.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import string
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / 'src'
DEFAULT_VC7 = HERE / 'toolchain' / 'vc7'
MODULES = ['defaultconvert', 'configstringconvert', 'xmodelalias', 'mptype', 'aitype', 'character', 'xanim', 'xmodel']

EXPECTED = {
    'bin/cl.exe': '05be307f04d8c3a9c211b77e85a63992be4642f14b4de31c04f9bb3d7bb1d65f',
    'bin/link.exe': '1560d85164dfb763dd13f90f33ff9c6f1cd80f2afada03eed8d0a314593581f1',
    'lib/libcmt.lib': '8815af7b9b6e0e28b77708ede25ab7ecfc4b05e1d8811f092c516cff5ce19d94',
}
CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def tool_env(vc7):
    env = os.environ.copy()
    for k in ('CL', '_CL_', 'LINK', '_LINK_', 'INCLUDE', 'LIB', 'LIBPATH'):
        env.pop(k, None)
    extra = [vc7 / 'bin']
    for rel in ('../Common7/IDE', '../Common7/Tools', '../Common7/Tools/bin'):
        d = (vc7 / rel).resolve()
        if d.is_dir():
            extra.append(d)
    env['PATH'] = os.pathsep.join([str(d) for d in extra] + [env.get('PATH', '')])
    return env


def run(cmd, cwd, env, timeout=600):
    r = subprocess.run(cmd, cwd=str(cwd), env=env, capture_output=True, timeout=timeout, creationflags=CREATE_NO_WINDOW)
    return r.returncode, (r.stdout + r.stderr).decode('mbcs' if os.name == 'nt' else 'utf-8', 'replace')


class RootedDrive:
    """A temporary drive letter for sources whose original __FILE__ was a rooted path such as \\trees\\...\\q_shared.c.

    cl.exe records the path it was given. A rooted path (leading backslash, no drive letter) only resolves when the
    current drive holds that directory tree, so the source is staged under build/root and a free drive letter is
    mapped to it with `subst` for the duration of the build. The letter itself never appears in the output.
    """

    def __init__(self, root_dir):
        self.root_dir = root_dir
        self.drive = None

    def ensure(self):
        if self.drive:
            return self.drive
        self.root_dir.mkdir(parents=True, exist_ok=True)
        for letter in reversed(string.ascii_uppercase[3:]):
            drive = letter + ':'
            if os.path.exists(drive + '\\'):
                continue
            r = subprocess.run(['subst', drive, str(self.root_dir)], capture_output=True, text=True,
                               creationflags=CREATE_NO_WINDOW)
            if r.returncode == 0:
                self.drive = drive
                return drive
        raise SystemExit('no free drive letter for subst')

    def release(self):
        if self.drive:
            subprocess.run(['subst', self.drive, '/D'], capture_output=True, creationflags=CREATE_NO_WINDOW)
            self.drive = None


def compile_all(man, vc7, sdk, out, env, rooted):
    (out / 'obj').mkdir(parents=True, exist_ok=True)
    for tu in man['translation_units']:
        src = ROOT / tu['source']
        work = out / 'work' / tu['obj'][:-4]
        work.mkdir(parents=True, exist_ok=True)
        obj = out / 'obj' / tu['obj']
        # __FILE__ is the name cl is given. The original project passed each file relative to its directory, so it is
        # compiled from its directory by bare name. "compile_path" names the original path for the few that differ.
        cwd, name = src.parent, src.name
        cp = tu.get('compile_path')
        if cp and cp.startswith('\\'):
            staged = rooted.root_dir / cp.lstrip('\\')
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_bytes(src.read_bytes())
            cwd, name = Path(rooted.ensure() + os.path.dirname(cp)), cp
        elif cp:
            staged = work / 'stage' / cp
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_bytes(src.read_bytes())
            cwd, name = work / 'stage', cp
        cmd = [str(vc7 / 'bin' / 'cl.exe'), *man['compile_flag_sets'][tu['flags']], '/I' + str(src.parent),
               '/I' + str(SRC), '/I' + str(vc7 / 'include'), '/I' + str(sdk / 'include'),
               '/Fo' + str(obj), '/Fd' + str(work / 'compiler.pdb'), name]
        # VC7's automatic precompiled header hangs when TMP/TEMP are overridden; give each object its own PCH path.
        if '/YX' in man['compile_flag_sets'][tu['flags']]:
            # Always start without a precompiled header: reusing one from an earlier build changes the order in which
            # templates are emitted, so a second build would differ from the first.
            (work / 'auto.pch').unlink(missing_ok=True)
            cmd.insert(-1, '/Fp' + str(work / 'auto.pch'))
            e = dict(env)
        else:
            e = dict(env, TMP=str(work), TEMP=str(work))
        code, text = run(cmd, cwd, e)
        (work / 'compile.log').write_text(text, encoding='utf-8')
        if code or not obj.exists():
            raise SystemExit('compile failed: %s\n%s' % (tu['source'], text))
        print('compiled', tu['source'])


def link_all(man, vc7, sdk, out, env):
    dll = out / (man['module'] + '.dll')
    archived = set()
    archives = {}
    for name, members in man.get('static_libraries', {}).items():
        library = out / (name + '.lib')
        cmd = [str(vc7 / 'bin' / 'lib.exe'), '/nologo', '/OUT:' + str(library)]
        cmd += [str(out / 'obj' / member) for member in members]
        code, text = run(cmd, out, env)
        if code:
            raise SystemExit('archive failed: %s\n%s' % (name, text))
        archived.update(members)
        archives['@' + name] = str(library)
    flags = list(man['link_flags']) + ['/OUT:' + str(dll), '/MAP:' + str(out / (man['module'] + '.map')),
                                       '/LIBPATH:' + str(out), '/LIBPATH:' + str(vc7 / 'lib'), '/LIBPATH:' + str(sdk / 'lib')]
    inputs = [str(out / 'obj' / t['obj']) for t in man['translation_units'] if t['obj'] not in archived]
    libraries = [archives.get(name, name) for name in man.get('link_libraries', [])]
    rsp = out / (man['module'] + '.rsp')
    rsp.write_text('\n'.join('"' + a + '"' for a in flags + inputs + libraries) + '\n', encoding='ascii')
    code, text = run([str(vc7 / 'bin' / 'link.exe'), '@' + str(rsp)], out, env)
    (out / 'link.log').write_text(text, encoding='utf-8')
    if code:
        print(text[-4000:])
        raise SystemExit('link.exe failed (exit %d)' % code)
    return dll


def pe_sections(path):
    d = Path(path).read_bytes()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec, = struct.unpack_from('<H', d, pe + 6)
    optsz, = struct.unpack_from('<H', d, pe + 20)
    out = {}
    for i in range(nsec):
        o = pe + 24 + optsz + 40 * i
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        vs, va, rs, rp = struct.unpack_from('<IIII', d, o + 8)
        out[name] = (vs, va, rs, d[rp:rp + rs])
    return d, out


def report(ours, original):
    a, b = Path(ours).read_bytes(), Path(original).read_bytes()
    n = min(len(a), len(b))
    diff = [i for i in range(n) if a[i] != b[i]]
    lines = ['file size : ours %d, original %d' % (len(a), len(b)),
             'file bytes: %d differing in common part%s' % (len(diff), (', first at 0x%X, last at 0x%X' % (diff[0], diff[-1])) if diff else '')]
    _, sa = pe_sections(ours)
    _, sb = pe_sections(original)
    for name in sorted(set(sa) | set(sb)):
        if name not in sa or name not in sb:
            lines.append('%-7s only in %s' % (name, 'ours' if name in sa else 'original'))
            continue
        x, y = sa[name], sb[name]
        m = min(len(x[3]), len(y[3]))
        dd = [i for i in range(m) if x[3][i] != y[3][i]]
        lines.append('%-7s vsize ours 0x%X / orig 0x%X  raw 0x%X / 0x%X  differing %d%s' % (
            name, x[0], y[0], x[2], y[2], len(dd), (' first +0x%X' % dd[0]) if dd else ''))
    return '\n'.join(lines)


def stamp(data, policy):
    """Restore the link timestamp: the COFF header field and the export directory's copy of it.

    These are the only fields link.exe cannot reproduce (it writes the time of the link). The DLLs have no
    debug directory, so there is no GUID or age to restore.
    """
    d = bytearray(data)
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    struct.pack_into('<I', d, pe + 8, policy['timestamp'])
    nsec, = struct.unpack_from('<H', d, pe + 6)
    optsz, = struct.unpack_from('<H', d, pe + 20)
    export_rva, = struct.unpack_from('<I', d, pe + 24 + 96)
    for i in range(nsec):
        o = pe + 24 + optsz + 40 * i
        vs, va, rs, rp = struct.unpack_from('<IIII', d, o + 8)
        if export_rva and va <= export_rva < va + max(vs, rs):
            struct.pack_into('<I', d, rp + export_rva - va + 4, policy['timestamp'])
    return bytes(d)


def build_module(module, args, vc7, rooted):
    man = json.loads((HERE / 'sources' / (module + '.json')).read_text(encoding='utf-8'))
    sdk = vc7 / 'PlatformSDK'
    out = (Path(args.out).resolve() / module) if args.out else HERE / 'out' / module
    out.mkdir(parents=True, exist_ok=True)
    env = tool_env(vc7)
    compile_all(man, vc7, sdk, out, env, rooted)
    dll = link_all(man, vc7, sdk, out, env)
    result = dll
    if args.identical:
        result = out / (module + '_identical.dll')
        result.write_bytes(stamp(dll.read_bytes(), man['stamp']))
    digest = sha256(result)
    print('%s sha256   %s' % (module, digest))
    print('%s original %s' % (module, man['original_sha256']))
    if digest == man['original_sha256']:
        print('RESULT: %s.dll IDENTICAL to the original' % module)
        return True
    print('RESULT: %s.dll differs from the original' % module)
    orig = Path(args.original_dir) / (module + '.dll') if args.original_dir else None
    if orig and orig.is_file():
        rep = report(result, orig)
        (out / 'diff_report.txt').write_text(rep + '\n', encoding='utf-8')
        print(rep)
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--module', default='all', help='one of %s, or all (default)' % ', '.join(MODULES))
    ap.add_argument('--vc7', default=str(DEFAULT_VC7), help='Visual C++ 7.0 Vc7 folder (default: build/toolchain/vc7)')
    ap.add_argument('--fetch-toolchain', action='store_true', help='run build/fetch_toolchain.py first if needed')
    ap.add_argument('--out', help='output folder (default: build/out/<module>)')
    ap.add_argument('--identical', action='store_true', help='also write <module>_identical.dll with the original link timestamp')
    ap.add_argument('--original-dir', help='folder with your original DLLs, for a difference report when a build differs')
    ap.add_argument('--strict', action='store_true', help='treat a toolchain hash mismatch as an error')
    args = ap.parse_args()

    modules = MODULES if args.module == 'all' else [args.module]
    for m in modules:
        if m not in MODULES:
            raise SystemExit('unknown module: ' + m)
    vc7 = Path(args.vc7).resolve()
    if args.fetch_toolchain and not (vc7 / 'bin' / 'cl.exe').is_file():
        r = subprocess.run([sys.executable, '-I', str(HERE / 'fetch_toolchain.py')])
        if r.returncode:
            raise SystemExit('toolchain fetch failed')
    bad = [r for r in EXPECTED if not (vc7 / r).is_file() or sha256(vc7 / r) != EXPECTED[r]]
    if bad:
        print('WARNING: toolchain files differ from the verified Visual C++ 7.0 install:', ', '.join(bad))
        if args.strict:
            raise SystemExit('--strict: toolchain mismatch')
    rooted = RootedDrive(HERE / 'root')
    results = {}
    try:
        for m in modules:
            results[m] = build_module(m, args, vc7, rooted)
    finally:
        rooted.release()
    if len(modules) > 1:
        for m in modules:
            print('%-20s %s' % (m, 'IDENTICAL' if results[m] else 'differs'))
    return 0 if all(results.values()) else 1


if __name__ == '__main__':
    sys.exit(main())

"""Attach license notices and the matching source ZIP without changing game files.

Usage: python package_licenses.py RUNTIME.zip SOURCE.zip --output OUTPUT.zip
No network access, game installation, or Nexus account changes are performed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import os
import tempfile
import zipfile

WORKSPACE = Path(__file__).resolve().parents[1]
GPL = WORKSPACE / 'COPYING.txt'
if not GPL.is_file():
    GPL = WORKSPACE / 'Pickpocketing-Loot/COPYING.txt'


def clean_source_archive(source, output=None):
    """Remove distribution artifacts, retaining mod/dependency code and notices."""
    source = Path(source).resolve()
    output = Path(output).resolve() if output else source
    output.parent.mkdir(parents=True, exist_ok=True)
    original_size = source.stat().st_size
    removed, retained_hashes, library = [], {}, None
    with zipfile.ZipFile(source) as z:
        entries = z.infolist()
        if any(n.endswith('/SOURCE-CLEANUP.json') for n in z.namelist()):
            assert z.testzip() is None
            if source != output:
                import shutil
                shutil.copyfile(source, output)
            return {'file': str(output), 'before_bytes': original_size, 'after_bytes': output.stat().st_size, 'already_clean': True}
        prefix = entries[0].filename.split('/')[0]
        def keep(name):
            p = Path(name)
            parts = p.parts[1:]
            if not parts or name.endswith('/'): return False
            legal = p.name.lower().startswith(('license', 'copying', 'copyright', 'exceptions')) or p.name == 'THIRD_PARTY.md'
            if legal: return True
            if parts[0] in ('artwork', 'logs', 'output', 'releases', 'build', '.git', '.gradle'): return False
            if '__pycache__' in parts or p.suffix.lower() in ('.pyc', '.pdb', '.obj', '.exp'): return False
            if parts[0] == 'Data' and p.suffix.lower() in ('.dll', '.lib', '.pex', '.esp', '.seq'): return False
            if 'vendor' in parts:
                if p.name == 'CommonLibSSE.lib': return False
                if any(part in ('tests', 'test', 'docs', 'doc', '.github', 'examples', 'benchmarks') for part in parts): return False
                if 'skyui' in parts and p.suffix.lower() != '.psc': return False
            return True
        handle, temporary = tempfile.mkstemp(prefix='source-clean-', suffix='.zip', dir=output.parent)
        os.close(handle)
        try:
            with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as cleaned:
                for info in entries:
                    if info.filename.endswith('/'): continue
                    if not keep(info.filename):
                        removed.append(info.filename)
                        if info.filename.endswith('/lib/CommonLibSSE.lib'):
                            library = (info.filename.split('/', 1)[1], hashlib.sha256(z.read(info)).hexdigest())
                        continue
                    data = z.read(info)
                    retained_hashes[info.filename] = hashlib.sha256(data).hexdigest()
                    cleaned.writestr(info.filename, data)
                if library:
                    relative, digest = library
                    script = f'''param([Parameter(Mandatory=$true)][string]$CommonLibArchive)
$ErrorActionPreference = 'Stop'
# Use the official v12.1.0 all-msvc-cmake release ZIP. Its required original
# library hash is checked before restoring it. No executable is launched.
$unpack = Join-Path $PSScriptRoot 'build/commonlib-restore'
New-Item -ItemType Directory -Force $unpack | Out-Null
Expand-Archive -LiteralPath $CommonLibArchive -DestinationPath $unpack -Force
$candidate = Get-ChildItem -LiteralPath $unpack -Recurse -Filter CommonLibSSE.lib | Where-Object {{ (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq '{digest}' }} | Select-Object -First 1
if (!$candidate) {{ throw 'The archive does not contain the matching CommonLibSSE library.' }}
$destination = Join-Path $PSScriptRoot '{relative}'
New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
Copy-Item -LiteralPath $candidate.FullName -Destination $destination -Force
Write-Output 'Matching CommonLib static library restored. Run Build-All.ps1 with your MSVC toolchain.'
'''
                    cleaned.writestr(prefix + '/Restore-CommonLib.ps1', script)
                readme = '''Source bundle cleanup

Mod source, Papyrus source, configuration, build scripts, dependency source and
license notices are retained. Precompiled mod files, release ZIPs, artwork,
logs, vendor tests/examples/docs, and unused SkyUI assets are omitted.
Included source files are byte-for-byte unchanged.

CommonLib source builds: configure with -DBUILD_TESTS=OFF because vendor test
fixtures are omitted. Mod-owned tests are retained.
Build tools/compiler/SDK dependencies remain as documented in the project.
'''
                if library:
                    readme += '''
The large prebuilt CommonLibSSE.lib is deliberately omitted. Before using the
original Build-Native.ps1, obtain commonlibsse-ng-prebuilt-v12.1.0-all-msvc-cmake.zip
from https://github.com/alandtse/CommonLibSSE-NG/releases/tag/v12.1.0
and run ./Restore-CommonLib.ps1 -CommonLibArchive <downloaded ZIP path>.
The helper verifies the original library SHA-256 before copying it into place.
No download or restoration is performed during packaging. The full CommonLib
implementation remains included under vendor/CommonLibSSE-NG for source builds.
Compiler and Papyrus tool requirements remain as documented in Build-All.ps1.
'''
                cleaned.writestr(prefix + '/SOURCE-BUILD.txt', readme)
                cleaned.writestr(prefix + '/SOURCE-CLEANUP.json', json.dumps({'removed': removed, 'retained_file_sha256': retained_hashes}, indent=2))
            with zipfile.ZipFile(temporary) as check:
                assert check.testzip() is None
                for name, digest in retained_hashes.items():
                    assert hashlib.sha256(check.read(name)).hexdigest() == digest
            z.close() # Windows cannot replace an archive while its reader is open.
            os.replace(temporary, output)
        finally:
            if Path(temporary).exists(): Path(temporary).unlink()
    return {'file': str(output), 'before_bytes': original_size, 'after_bytes': output.stat().st_size,
            'removed_files': len(removed), 'retained_files_verified': len(retained_hashes)}


def package(runtime, source, output=None, source_url=None):
    runtime, source = Path(runtime).resolve(), Path(source).resolve()
    output = Path(output).resolve() if output else runtime
    assert runtime != source and output != source
    output.parent.mkdir(parents=True, exist_ok=True)
    clean_source_archive(source)
    license_bytes = GPL.read_bytes()
    assert b'GNU GENERAL PUBLIC LICENSE' in license_bytes and b'END OF TERMS AND CONDITIONS' in license_bytes
    repositories = ('Dynamic-Pickpocket-Loot','Standing-Pickpocket','Harvestable-Flora-Regrows','NPCs-Have-Ammo','Notes-and-Letters')
    if source_url is None:
        name = next((name for name in repositories if runtime.name.startswith(name)),None)
        if not name: raise ValueError('Provide the published GitHub source URL for this mod.')
        version = re.search(r'-v(\d+\.\d+\.\d+)',runtime.name)
        if not version: raise ValueError('Versioned runtime archive required')
        tag = 'source-' + version[1] + ('-dev' if name == 'Notes-and-Letters' else '')
        source_url = 'https://github.com/mc-hambers/' + name + '/tree/' + tag
    if not source_url.startswith('https://github.com/'): raise ValueError('Expected a GitHub source URL')
    notice = f'''Copyright (C) 2026 Maxmc

This mod's original code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.

Full license: LICENSE.txt
Corresponding source, dependency sources, build scripts, and original notices are hosted on GitHub:
{source_url}

Third-party components retain their original copyrights and licenses.
CommonLibSSE-NG permissions and exceptions are preserved in the source archive
and reproduced under Third-Party-Licenses. This notice does not relicense any
third-party material. Build tools and game dependencies must be obtained
separately where the included build instructions require them.

This is a packaging-only update. Game files and version numbers are unchanged.
Legal notices do not need to be installed into the game Data folder.
Nexus page permissions must separately allow modification and redistribution
under the license terms. This tool does not change the Nexus page.
'''.encode('utf-8')
    with zipfile.ZipFile(source) as src:
        assert src.testzip() is None, 'Corrupt source archive'
        assert any(n.endswith(('.cpp', '.h')) and '/src/' in n for n in src.namelist()), 'Missing mod source'
        assert any('vendor/CommonLibSSE-NG/' in n for n in src.namelist()), 'Missing CommonLib source'
        legal = {}
        for n in src.namelist():
            p = Path(n)
            if p.name.lower().startswith(('license', 'copying', 'copyright', 'exceptions')) or ('/third-party/' in n and not n.endswith('/')) or p.name == 'THIRD_PARTY.md':
                if not n.endswith('/'):
                    # Keep original directory context to distinguish dependency notices.
                    legal['Third-Party-Licenses/' + n] = src.read(n)
    with zipfile.ZipFile(runtime) as original:
        assert original.testzip() is None
        files = {n: original.read(n) for n in original.namelist()
                 if not n.endswith('/') and not n.startswith(('Docs/', 'Source/', 'Third-Party-Licenses/'))
                 and n not in ('LICENSE', 'LICENSE.txt', 'README.txt', 'PACKAGE-CONTENTS.json')}
    manifest = {n: hashlib.sha256(data).hexdigest() for n, data in files.items()}
    handle, temporary = tempfile.mkstemp(prefix='licensed-', suffix='.zip', dir=output.parent)
    os.close(handle)
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for name, data in files.items(): z.writestr(name, data)
            z.writestr('LICENSE.txt', license_bytes)
            z.writestr('README.txt', notice)
            for name, data in legal.items(): z.writestr(name, data)
        with zipfile.ZipFile(temporary) as z:
            assert z.testzip() is None
            assert len(z.namelist()) == len(set(z.namelist()))
            assert not any(n.startswith('Docs/') for n in z.namelist())
            assert z.read('LICENSE.txt') == license_bytes
            for name, expected in manifest.items():
                with z.open(name) as stream:
                    assert hashlib.file_digest(stream, 'sha256').hexdigest() == expected, name
        os.replace(temporary, output)
    finally:
        if Path(temporary).exists(): Path(temporary).unlink()
    return {'file': str(output), 'bytes': output.stat().st_size,
            'sha256': hashlib.file_digest(output.open('rb'), 'sha256').hexdigest(),
            'unchanged_payload_files': len(files), 'third_party_notices': len(legal),
            'source_url': source_url, 'source_embedded': False, 'zip_integrity': 'passed',
            'runtime_and_source_hashes': 'passed', 'docs_folder': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime')
    parser.add_argument('source')
    parser.add_argument('--output')
    parser.add_argument('--source-url')
    args = parser.parse_args()
    print(json.dumps(package(args.runtime, args.source, args.output, args.source_url), indent=2))

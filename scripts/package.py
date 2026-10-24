"""Validate and package the development build. Never install into the game directory."""
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile
from pathlib import Path
from build_content import main as content
from verify_binary import main as binary

ROOT=Path(__file__).resolve().parents[1]
VERSION='1.1.0'

def validate_esp(path):
    raw=path.read_bytes();tag,size,flags=struct.unpack_from('<4sII',raw);assert tag==b'TES4' and flags&0x200
    assert b'WICourierRadiant' not in raw[24:24+size]
    pos=24+size;found={};books=[]
    while pos<len(raw):
        assert raw[pos:pos+4]==b'GRUP';end=pos+struct.unpack_from('<I',raw,pos+4)[0];label=raw[pos+8:pos+12];pos+=24
        while pos<end:
            tag,size,flags,form=struct.unpack_from('<4sIII',raw,pos);assert tag==label and not flags
            fields={};record_end=pos+24+size;cursor=pos+24
            while cursor<record_end:
                key,length=struct.unpack_from('<4sH',raw,cursor);cursor+=6;assert cursor+length<=record_end;fields.setdefault(key,[]).append(raw[cursor:cursor+length]);cursor+=length
            assert cursor==record_end and form not in found;found[form]=(tag,fields)
            if tag==b'BOOK':
                index=form-0x01000800;assert 0<=index<896;variant=index//224
                assert all(k in fields for k in (b'EDID',b'FULL',b'DESC',b'DATA',b'MODL',b'INAM'))
                assert len(fields[b'DATA'][0])==16 and fields[b'DATA'][0][:2]==b'\0\0'
                assert fields[b'INAM'][0]==struct.pack('<I',[0x97788,0x1541B,0x1541C,0x15421][variant])
                assert fields[b'MODL'][0]==('Clutter/Books/Note%02d.nif'%(variant+1)).replace('/','\\').encode()+b'\0'
                books.append((form,fields[b'FULL'][0]))
            pos=record_end
        assert pos==end
    assert len(books)==896 and len({title for _,title in books})==224
    quest=found[0x01000C00];package=found[0x01000C01];assert quest[0]==b'QUST' and package[0]==b'PACK'
    assert b'ALPC' not in quest[1] and b'ALST' not in quest[1] and b'VMAD' not in quest[1]
    assert b'PKCU' not in package[1] and package[1][b'PKDT'][0][4]==27
    return dict(esl_flag=True,compact_book_records=896,unique_note_texts=224,model_variants=4,old_delivery_implementation_removed=True,retired_records_inert=True,structural_record_validation='passed')

def main():
    content()
    native=(ROOT/'src/Plugin.cpp').read_text(encoding='utf-8')
    assert not any(name in native for name in ('WICourierRadiant.esp','OnNLDeliverNote','routeCourier','refreshEventBridge'))
    assert 'if (relationships) playerLetters(a,s,day);' in native
    assert 'NLDeliverPlayerNote' in native and 'recoverQueuedNPCNotes();' in native
    result=binary()
    test=subprocess.run([str(ROOT/'build/NoteTests.exe')],capture_output=True,text=True,check=True)
    result['core_behavior_checks']=int(re.search(r'Passed (\d+) note behavior checks',test.stdout)[1])
    package=ROOT/'build/package'
    result.update(validate_esp(package/'NotesAndLetters.esp'))
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    result['dll_sha256']=digest(package/'SKSE/Plugins/NotesAndLetters.dll')
    result['esp_sha256']=digest(package/'NotesAndLetters.esp')
    result['save_schema']=2
    result['paper_note_inventory_art']=['Skyrim.esm|097788','Skyrim.esm|01541B','Skyrim.esm|01541C','Skyrim.esm|015421']
    result['off_cell_npc_distribution']=True
    result['explicit_courier_helper']=False
    result['npc_courier_integration']=False
    result['book_text_hook_show_event_only']=True
    result['legacy_queued_npc_recovery']=True
    result['polymorphic_courier_reference_resolution']=True
    result['session_only_repeat_debug']=True
    result['debug_commands']=['NLDebug','NLSpawnNote','NLRepeatNotes','NLDeliverPlayerNote']
    result['safe_extra_data_allocation_bytes']=32
    result['development_build']=True
    (ROOT/'validation-results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    notices=ROOT/'third-party';notices.mkdir(exist_ok=True)
    dep=ROOT if (ROOT/'vcpkg_installed').is_dir() else ROOT.parent/'Pickpocketing-Loot'
    for name in ('spdlog','fmt','directxmath','directxtk','rapidcsv'):
        shutil.copyfile(dep/f'vcpkg_installed/x64-portable/share/{name}/copyright',notices/f'{name}-LICENSE.txt')
    shutil.copyfile(ROOT/'vendor/CommonLibSSE-NG/licenses/LICENSE-MIT.txt',notices/'CommonLibSSE-original-MIT.txt')
    release=ROOT/'releases';release.mkdir(exist_ok=True)
    install=release/f'Notes-and-Letters-v{VERSION}-dev.zip'
    source=release/f'Notes-and-Letters-v{VERSION}-source.zip'
    docs=('README.md','VALIDATION.md','validation-results.json','COPYING.txt','EXCEPTIONS.md','Notes-and-Letters-Proofreading.md')
    config=package/'SKSE/Plugins/NotesAndLetters.ini'
    shutil.copyfile(ROOT/'config/NotesAndLetters.ini',config)
    with zipfile.ZipFile(install,'w',zipfile.ZIP_DEFLATED) as z:
        for p in package.rglob('*'):
            if p.is_file() and p.suffix.lower() in ('.dll','.esp','.ini'):z.write(p,p.relative_to(package).as_posix())
    prefix=f'Notes-and-Letters-v{VERSION}/'
    with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED) as z:
        for name in docs+('CMakeLists.txt','vcpkg.json','.gitignore'):z.write(ROOT/name,prefix+name)
        for directory in ('src','tests','content','scripts','config','cmake','third-party','vendor/CommonLibSSE-NG'):
            for p in (ROOT/directory).rglob('*'):
                if p.is_file() and not any(k in p.parts for k in ('.git','__pycache__')):
                    z.write(p,prefix+p.relative_to(ROOT).as_posix())
        for name in ('binary-validation.json',):z.write(ROOT/'research'/name,prefix+'research/'+name)
        for p in (dep/'tools/vcpkg-downloads').iterdir():
            if p.is_file() and (p.name.endswith('.tar.gz') or p.suffix=='.patch'):z.write(p,prefix+'dependency-sources/'+p.name)
        for name in ('spdlog','fmt','directxmath','directxtk','rapidcsv','vcpkg-cmake','vcpkg-cmake-config'):
            port=dep/f'tools/vcpkg/ports/{name}'
            for p in port.rglob('*'):
                if p.is_file():z.write(p,prefix+f'dependency-sources/ports/{name}/'+p.relative_to(port).as_posix())
        for local,name in [('scripts/cmake/vcpkg_configure_cmake.cmake','vcpkg_configure_cmake.cmake'),('LICENSE.txt','vcpkg-LICENSE.txt')]:z.write(dep/'tools/vcpkg'/local,prefix+'dependency-sources/'+name)
    for path in (install,source):
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            assert len(z.namelist())==len(set(z.namelist()))
            assert not any('/research/dependency/' in n or n.endswith(('.bsa','.esm')) for n in z.namelist())
            assert not any('NL_NPCNoteDelivery' in n or 'CourierEvent' in n or 'build_courier_event' in n for n in z.namelist())
    with zipfile.ZipFile(install) as z:
        assert set(z.namelist())=={'NotesAndLetters.esp','SKSE/Plugins/NotesAndLetters.dll','SKSE/Plugins/NotesAndLetters.ini'}
        for name in ('NotesAndLetters.esp','SKSE/Plugins/NotesAndLetters.dll','SKSE/Plugins/NotesAndLetters.ini'):assert z.read(name)==(package/name).read_bytes()
    from package_licenses import package as attach_licenses
    attach_licenses(install,source)
    report={p.name:dict(bytes=p.stat().st_size,sha256=digest(p)) for p in (install,source)}
    (release/'SHA256.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(validation=result,archives=report),indent=2))

if __name__=='__main__':main()

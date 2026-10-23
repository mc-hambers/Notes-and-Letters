"""Build original BOOK records in an ESL-flagged ESP and a native catalog header."""
import html
import json
import struct
from pathlib import Path
from render_proof import main as proof
ROOT = Path(__file__).resolve().parents[1]

def sub(tag, data):
    assert len(data) < 65536
    return tag.encode() + struct.pack('<H', len(data)) + data

def text(tag, value):
    return sub(tag, value.encode('utf-8') + b'\0')

def record(tag, form, data, flags=0):
    return struct.pack('<4sIIIIHH', tag.encode(), len(data), flags, form, 0, 44, 0) + data

def main():
    proof()
    notes = json.loads((ROOT/'content/notes.json').read_text(encoding='utf-8'))
    keys = list(dict.fromkeys(n['category'] for n in notes))
    books = b''
    models=[(r'Clutter\Books\Note01.nif',0x97788),(r'Clutter\Books\Note02.nif',0x1541B),(r'Clutter\Books\Note03.nif',0x1541C),(r'Clutter\Books\Note04.nif',0x15421)]
    header = ['#pragma once', '#include <array>', '#include <string_view>', 'namespace NL {',
              'struct Text { std::string_view id,title,body,signature; unsigned category; };',
              'inline constexpr std::array<Text,224> catalog{{']
    for i, n in enumerate(notes):
        fallback = n['body'].replace('{player_name}', 'To you').replace('{shop}', 'your shop')
        body = "<font face='$HandwrittenFont'><p align='left'>" + html.escape(fallback).replace('\n','<br>') + '</p></font>'
        for variant,(model,art) in enumerate(models):
            suffix='' if variant==0 else '_Paper'+str(variant+1)
            fields = text('EDID', 'NL_'+n['id'].replace('-','_')+suffix) + sub('OBND', struct.pack('<6h', -8,-5,0,8,5,1))
            fields += text('FULL', n['title']) + text('MODL',model)
            fields += text('DESC', body) + sub('DATA',struct.pack('<BBHiIf',0,0,0,-1,1,0.1)) + sub('INAM',struct.pack('<I',art))
            books += record('BOOK',0x01000800+variant*224+i,fields)
        vals = ','.join(json.dumps(n[k], ensure_ascii=True) for k in ('id','title','body','signature'))
        header.append('    Text{'+vals+','+str(keys.index(n['category']))+'},')
    header += ['}};', '}']
    (ROOT/'src').mkdir(exist_ok=True)
    (ROOT/'src/Catalog.h').write_text('\n'.join(header)+'\n', encoding='utf-8')
    master = text('MAST','Skyrim.esm') + sub('DATA',b'\0'*8)
    records=[];pos=0
    while pos<len(books):
        size=struct.unpack_from('<I',books,pos+4)[0];records.append(books[pos:pos+24+size]);pos+=24+size
    books=b''.join(sorted(records,key=lambda row:struct.unpack_from('<I',row,12)[0]))
    # Inert compatibility records let updates retire the old quest without missing FormIDs.
    quest=text('EDID','NLRetiredDeliveryHelper')+sub('DNAM',bytes.fromhex('000000000000000000000000'))+sub('NEXT',b'')+sub('ANAM',struct.pack('<I',0))
    package=text('EDID','NLRetiredDeliveryPackage')+sub('PKDT',bytes.fromhex('000000001b000000ffff0000'))+sub('PSDT',bytes.fromhex('ffff00ffff00000000000000'))
    def group(tag,contents):return struct.pack('<4sI4sIHHHH',b'GRUP',24+len(contents),tag.encode(),0,0,0,0,0)+contents
    metadata=sub('HEDR',struct.pack('<fII',1.7,898,0xC02))+text('CNAM','Maxmc')+text('SNAM','Notes and Letters: original correspondence catalog')+master
    data=record('TES4',0,metadata,0x200)+group('BOOK',books)+group('QUST',record('QUST',0x01000C00,quest))+group('PACK',record('PACK',0x01000C01,package))
    destination=ROOT/'build/package'
    destination.mkdir(parents=True,exist_ok=True)
    (destination/'NotesAndLetters.esp').write_bytes(data)
    print('Built 224 original notes with 896 stable model variants and inert legacy records in NotesAndLetters.esp (ESL flag, compact IDs).')

if __name__=='__main__':main()

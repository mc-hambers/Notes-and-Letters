"""File and Windows-loader checks; does not call SKSEPlugin_Load or launch Skyrim."""
import ctypes
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
    path=ROOT/'build/package/SKSE/Plugins/NotesAndLetters.dll'
    raw=path.read_bytes();pe=struct.unpack_from('<I',raw,0x3c)[0]
    assert raw[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',raw,pe+4)[0]==0x8664
    dll=ctypes.WinDLL(str(path.resolve()))
    for symbol in ('SKSEPlugin_Load','SKSEPlugin_Query'):getattr(dll,symbol)
    version=(ctypes.c_uint32*2).in_dll(dll,'SKSEPlugin_Version')
    assert version[1]==0x01000000
    name=ctypes.string_at(ctypes.addressof(version)+8,256).split(b'\0',1)[0].decode()
    assert name=='Notes and Letters'
    report=dict(version='1.0.0',plugin_name=name,dll_architecture='x64',windows_loader='passed',skse_exports='passed',gameplay_tested=False)
    (ROOT/'research/binary-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report

if __name__=='__main__':print(json.dumps(main(),indent=2))

"""Deterministic generated files, plus separately sourced real-world fixtures."""
from pathlib import Path
import struct, zlib, json, zipfile, wave, random, os
p = Path(__file__).parent / 'fixtures'
p.mkdir(exist_ok=True)
def chunk(tag, data):
    tag = tag.encode('ascii')
    return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
header = b'\x89PNG\r\n\x1a\n'+chunk('IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))
end = chunk('IDAT',zlib.compress(b'\0\xff\0\0'))+chunk('IEND',b'')
def png(name, chunks=b'', trailing=b''):
    (p / name).write_bytes(header+chunks+end+trailing)
params='a lighthouse at dawn\nNegative prompt: text, watermark\nSteps: 24, Sampler: Euler, CFG scale: 7, Seed: 1234, Size: 1x1, Model: fixture, Model hash: 0123456789, ControlNet 0: "preprocessor: canny, model: test"'
workflow={"last_node_id":1,"nodes":[{"id":1,"type":"KSampler","widgets_values":[1234,24,7,"euler"]}],"links":[]}
prompt={"1":{"class_type":"KSampler","inputs":{"seed":1234,"steps":24,"cfg":7,"sampler_name":"euler","scheduler":"normal"}}}
png('metadata.png',chunk('tEXt',b'Author\0Fixture author')+chunk('tEXt',b'Author\0Conflicting author')+chunk('tEXt',b'parameters\0'+params.encode())+chunk('tEXt',b'workflow\0'+json.dumps(workflow).encode())+chunk('tEXt',b'prompt\0'+json.dumps(prompt).encode())+chunk('vpAg',b'unknown\x00block'),b'TRAILING-EVIDENCE')
png('clean.png')
png('malformed-c2pa.png',chunk('caBX',b'not a JUMBF manifest'))
compressed=zlib.compress(b'\0\xff\0\0')
(p/'repeated-idat.png').write_bytes(header+chunk('IDAT',compressed[:4])+chunk('IDAT',compressed[4:8])+chunk('IDAT',compressed[8:])+chunk('IEND',b''))
png('changed.png',chunk('tEXt',b'Author\0Other author'))
png('compressed.png',chunk('zTXt',b'Comment\0\0'+zlib.compress(b'Compressed metadata recovered by ExifTool')))
png('unicode-画像-é.png',chunk('iTXt','Description\0\0\0\0\0Unicode 画像 é'.encode('utf8')))
(p/'wrong.jpg').write_bytes((p/'clean.png').read_bytes())
bad=bytearray((p/'clean.png').read_bytes());bad[29]^=1;(p/'bad-crc.png').write_bytes(bad)
(p/'truncated.png').write_bytes(header+struct.pack('>I',0x7fffffff)+b'tEXt')
png('large-text.png',chunk('tEXt',b'Comment\0'+b'x'*(2*1024*1024)))
png('malformed-generation.png',chunk('tEXt',b'workflow\0'+b'['*200))
(p/'zero.bin').write_bytes(b'')
(p/'unknown.bin').write_bytes(b'abc')
(p/'malformed.jpg').write_bytes(b'\xff\xd8\xff\xe1\xff\xffbad')
(p/'no-scan.jpg').write_bytes(b'\xff\xd8\xff\xd9')
pdf=bytearray(b'%PDF-1.4\n'); offsets=[0]
for number,body in enumerate([b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Kids [] /Count 0 >>',b'<< /Author (Fixture author) /Title (Forensic fixture) >>'],1):
    offsets.append(len(pdf));pdf+=f'{number} 0 obj\n'.encode()+body+b'\nendobj\n'
xref=len(pdf);pdf+=b'xref\n0 4\n0000000000 65535 f \n'
for off in offsets[1:]:pdf+=f'{off:010} 00000 n \n'.encode()
pdf+=f'trailer\n<< /Size 4 /Root 1 0 R /Info 3 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode();(p/'fixture.pdf').write_bytes(pdf)
with zipfile.ZipFile(p/'fixture.docx','w') as z:
    z.writestr('[Content_Types].xml','<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>')
    z.writestr('word/document.xml','<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body/></w:document>')
    z.writestr('docProps/core.xml','<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:creator>Fixture author</dc:creator></cp:coreProperties>')
with zipfile.ZipFile(p/'fixture.zip','w') as z:
    z.writestr('evidence.txt','Never execute this file')
with wave.open(str(p/'fixture.wav'),'wb') as w:
    w.setparams((1,2,8000,0,'NONE','not compressed'));w.writeframes(b'\0\0'*80)
# >4 GiB sparse file proves 64-bit size/cancellation without disk-sized allocation.
with (p/'large-sparse.bin').open('wb') as f:
    if os.name == 'nt':
        import ctypes, msvcrt
        done=ctypes.c_ulong()
        ok=ctypes.windll.kernel32.DeviceIoControl(ctypes.c_void_p(msvcrt.get_osfhandle(f.fileno())),0x900c4,None,0,None,0,ctypes.byref(done),None)
        if not ok: raise ctypes.WinError()
    f.seek(5*1024**3);f.write(b'X')
r=random.Random(123)
for i in range(48):
    (p/f'fuzz-{i}.bin').write_bytes((b'\x89PNG\r\n\x1a\n' if i%2 else b'\xff\xd8\xff')+r.randbytes(i*71))
print(f'Generated fixtures in {p}')

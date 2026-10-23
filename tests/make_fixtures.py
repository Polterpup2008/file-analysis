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

# Synthetic EXIF/XMP camera claims. These are fixtures, not real capture evidence.
# An ordinary TIFF IFD tree is embedded unchanged in PNG eXIf and JPEG APP1.
def camera_tiff():
    data = bytearray(b'II\x2a\0\0\0\0\0')
    def ascii_value(value):
        payload = value.encode('ascii') + b'\0'
        return 2, len(payload), payload
    def short(value): return 3, 1, struct.pack('<H', value)
    def long(value): return 4, 1, struct.pack('<I', value)
    def rational(*values):
        return 5, len(values), b''.join(struct.pack('<II', *value) for value in values)
    def ifd(entries):
        if len(data) % 2: data.append(0)
        offset = len(data)
        entries = sorted(entries)
        data.extend(struct.pack('<H', len(entries)) + bytes(12 * len(entries) + 4))
        for index, (tag, (kind, count, payload)) in enumerate(entries):
            pos = offset + 2 + 12 * index
            struct.pack_into('<HHI', data, pos, tag, kind, count)
            if len(payload) <= 4: data[pos+8:pos+12] = payload.ljust(4, b'\0')
            else:
                if len(data) % 2: data.append(0)
                struct.pack_into('<I', data, pos+8, len(data))
                data.extend(payload)
        return offset
    gps = ifd([(0, (1, 4, b'\2\3\0\0')), (1, ascii_value('N')),
               (2, rational((0, 1), (0, 1), (0, 1))), (3, ascii_value('E')),
               (4, rational((0, 1), (0, 1), (0, 1)))])
    exif = ifd([(0x829a, rational((1, 125))), (0x829d, rational((28, 10))),
                (0x8827, short(200)), (0x9000, (7, 4, b'0232')),
                (0x9003, ascii_value('2024:06:15 12:34:56')),
                (0x9011, ascii_value('+02:00')), (0x9209, short(0)),
                (0x920a, rational((35, 1))), (0x9291, ascii_value('123')),
                (0xa001, short(1)), (0xa002, long(1)), (0xa003, long(1)),
                (0xa405, short(52)), (0xa431, ascii_value('FIXTURE-ONLY-001')),
                (0xa433, ascii_value('Fixture Optics')),
                (0xa434, ascii_value('35mm F2.8 fixture lens')),
                (0xa435, ascii_value('FIXTURE-LENS-002'))])
    root = ifd([(0x010f, ascii_value('Fixture Camera Co')),
                (0x0110, ascii_value('Fixture Phone 4')),
                (0x011a, rational((72, 1))), (0x011b, rational((72, 1))),
                (0x0128, short(2)), (0x8769, long(exif)), (0x8825, long(gps))])
    struct.pack_into('<I', data, 4, root)
    return bytes(data)

photo_exif = camera_tiff()
png('camera.png', chunk('eXIf', photo_exif))
(p/'camera.tiff').write_bytes(photo_exif)  # Metadata-only TIFF: no pixel decoding is claimed.
xmp_camera = b'''<x:xmpmeta xmlns:x="adobe:ns:meta/"><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"><rdf:Description rdf:about="" xmlns:tiff="http://ns.adobe.com/tiff/1.0/" tiff:Make="Other Fixture Co" tiff:Model="Conflicting Fixture Model"/></rdf:RDF></x:xmpmeta>'''
png('camera-conflict.png', chunk('eXIf', photo_exif) + chunk('iTXt', b'XML:com.adobe.xmp\0\0\0\0\0' + xmp_camera))
png('camera-xmp-only.png', chunk('iTXt', b'XML:com.adobe.xmp\0\0\0\0\0' + xmp_camera))

# Reuse only the ordinary JPEG image data from the pinned signed test asset;
# strip its APP1..APP15 metadata so this generated fixture has no C2PA claims.
signed_sample = p/'c2pa-signed.jpg'
if signed_sample.exists():
    original = signed_sample.read_bytes()
    jpeg_image = bytearray(original[:2]); position = 2
    while position < len(original):
        start = position
        if original[position] != 255: raise ValueError('Unexpected JPEG fixture marker')
        while original[position] == 255: position += 1
        marker = original[position]; position += 1
        if marker in (0xda, 0xd9):
            jpeg_image.extend(original[start:]); break
        length = struct.unpack_from('>H', original, position)[0]
        if not 0xe1 <= marker <= 0xef: jpeg_image.extend(original[start:position+length])
        position += length
    app1 = b'Exif\0\0' + photo_exif
    (p/'camera.jpg').write_bytes(jpeg_image[:2] + b'\xff\xe1' + struct.pack('>H', len(app1)+2) + app1 + jpeg_image[2:])

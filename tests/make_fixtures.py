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
# Valid framing with dimensions beyond the viewer budget; pixel decoding must
# be refused before expanding the deliberately short IDAT data.
(p/'oversize-preview.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk('IHDR',struct.pack('>IIBBBBB',16385,1,8,2,0,0,0))+end)
(p/'pixel-budget-preview.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk('IHDR',struct.pack('>IIBBBBB',8192,8192,8,2,0,0,0))+end)
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
invoke_record={"positive_prompt":"fixture lighthouse", "negative_prompt":"fixture text", "seed":0, "steps":24, "cfg_scale":6.5, "scheduler":"euler", "width":1024, "height":768, "model":{"name":"fixture model","hash":"fixture-model-hash"}, "loras":[{"model":{"name":"fixture lora"},"weight":0.6}], "future_property":{"kept":True}}
invoke_workflow={"nodes":[],"edges":[],"meta":{"version":"1.0.0"}}
png('invokeai.png',chunk('tEXt',b'invokeai_metadata\0'+json.dumps(invoke_record).encode())+chunk('tEXt',b'invokeai_workflow\0'+json.dumps(invoke_workflow).encode())+chunk('tEXt',b'invokeai_graph\0'+json.dumps({"nodes":{},"edges":[]}).encode()))
fooocus_record={"prompt":"fixture forest", "negative_prompt":"fixture watermark", "full_prompt":["fixture forest","fixture style"], "seed":0, "steps":30, "guidance_scale":7, "base_model":"fixture checkpoint", "base_model_hash":"fixture-hash", "resolution":"(1024, 768)", "loras":[["fixture-lora",0.7,"fixture-lora-hash"]], "scheduler":"karras"}
png('fooocus.png',chunk('tEXt',b'fooocus_scheme\0fooocus')+chunk('tEXt',b'parameters\0'+json.dumps(fooocus_record).encode()))
nai_record={"prompt":"fixture red boat", "uc":"fixture blur", "seed":0, "steps":28, "scale":5, "sampler":"k_euler", "width":1024, "height":1024, "v4_prompt":{"caption":{"base_caption":"fixture red boat"}}, "signed_hash":"UNVERIFIED-FIXTURE"}
png('novelai.png',chunk('tEXt',b'Software\0NovelAI')+chunk('tEXt',b'Comment\0'+json.dumps(nai_record).encode()))
png('ordinary-json-comment.png',chunk('tEXt',b'Comment\0'+json.dumps(nai_record).encode()))
png('broken-invokeai.png',chunk('tEXt',b'invokeai_metadata\0{broken-json'))
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
# Metadata-only ISO BMFF fixture assembled from the upstream QuickTime atoms.
# It tests container/metadata recognition; no playable movie is claimed.
if (p/'upstream-QuickTime.mov').exists():
    (p/'fixture.mp4').write_bytes(struct.pack('>I',24)+b'ftypisom'+struct.pack('>I',0)+b'isommp42'+(p/'upstream-QuickTime.mov').read_bytes())
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
def camera_tiff(make='Fixture Camera Co', model='Fixture Phone 4', canon_model_id=None, sony_model_id=None):
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
    exif_entries = [(0x829a, rational((1, 125))), (0x829d, rational((28, 10))),
                (0x8827, short(200)), (0x9000, (7, 4, b'0232')),
                (0x9003, ascii_value('2024:06:15 12:34:56')),
                (0x9011, ascii_value('+02:00')), (0x9209, short(0)),
                (0x920a, rational((35, 1))), (0x9291, ascii_value('123')),
                (0xa001, short(1)), (0xa002, long(1)), (0xa003, long(1)),
                (0xa405, short(52)), (0xa431, ascii_value('FIXTURE-ONLY-001')),
                (0xa433, ascii_value('Fixture Optics')),
                (0xa434, ascii_value('35mm F2.8 fixture lens')),
                (0xa435, ascii_value('FIXTURE-LENS-002'))]
    if canon_model_id is not None:
        note = struct.pack('<H', 1) + struct.pack('<HHII', 0x10, 4, 1, canon_model_id) + struct.pack('<I', 0)
        exif_entries.append((0x927c, (7, len(note), note)))
    if sony_model_id is not None:
        note = b'SONY DSC \0\0\0' + struct.pack('<H', 1) + struct.pack('<HHII', 0xb001, 3, 1, sony_model_id) + struct.pack('<I', 0)
        exif_entries.append((0x927c, (7, len(note), note)))
    exif = ifd(exif_entries)
    root_entries = [(0x011a, rational((72, 1))), (0x011b, rational((72, 1))),
                    (0x0128, short(2)), (0x8769, long(exif)), (0x8825, long(gps))]
    if make is not None: root_entries.append((0x010f, ascii_value(make)))
    if model is not None: root_entries.append((0x0110, ascii_value(model)))
    root = ifd(root_entries)
    struct.pack_into('<I', data, 4, root)
    return bytes(data)

# Sourced model-code relationships tested through the actual metadata reader.
for name, make, model in [('samsung', 'Samsung', 'SM-S918B'), ('apple', 'Apple', 'iPhone15,2'),
                          ('xiaomi', 'Xiaomi', '23021RAAEG')]:
    png('device-'+name+'.png', chunk('eXIf', camera_tiff(make, model)))
(p/'device-samsung.tiff').write_bytes(camera_tiff('Samsung', 'SM-S918B'))
png('device-canon-id.png', chunk('eXIf', camera_tiff('Canon', None, 0x80000175)))
png('device-sony-id.png', chunk('eXIf', camera_tiff('SONY', None, sony_model_id=388)))

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
    phone_app1 = b'Exif\0\0' + camera_tiff('Samsung', 'SM-S918B')
    (p/'device-samsung.jpg').write_bytes(jpeg_image[:2] + b'\xff\xe1' + struct.pack('>H', len(phone_app1)+2) + phone_app1 + jpeg_image[2:])

    def with_thumbnail(tiff, thumbnail):
        tiff = bytearray(tiff)
        root_offset = struct.unpack_from('<I', tiff, 4)[0]
        root_count = struct.unpack_from('<H', tiff, root_offset)[0]
        if len(tiff) % 2: tiff.append(0)
        thumbnail_ifd = len(tiff)
        thumbnail_offset = thumbnail_ifd + 2 + 3 * 12 + 4
        struct.pack_into('<I', tiff, root_offset + 2 + root_count * 12, thumbnail_ifd)
        tiff += struct.pack('<H', 3)
        tiff += struct.pack('<HHI4s', 0x0103, 3, 1, struct.pack('<H', 6) + b'\0\0')
        tiff += struct.pack('<HHII', 0x0201, 4, 1, thumbnail_offset)
        tiff += struct.pack('<HHII', 0x0202, 4, 1, len(thumbnail))
        tiff += struct.pack('<I', 0) + thumbnail
        return bytes(tiff)

    # Deterministic 160x96 gradient JPEG generated with Windows GDI+, no camera/author metadata.
    preview = __import__("base64").b64decode("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")
    (p/'embedded-preview.jpg').write_bytes(preview)
    embedded = with_thumbnail(photo_exif, preview)
    png('camera-preview.png', chunk('eXIf', embedded))
    preview_app1 = b'Exif\0\0' + embedded
    if len(preview_app1) + 2 <= 65535:
        (p/'camera-preview.jpg').write_bytes(jpeg_image[:2] + b'\xff\xe1' + struct.pack('>H', len(preview_app1)+2) + preview_app1 + jpeg_image[2:])
    png('bad-preview.png', chunk('eXIf', with_thumbnail(photo_exif, b'\xff\xd8\xff\xe0\x00\x04xx\xff\xd9')))
    png('unsupported-preview.png', chunk('eXIf', with_thumbnail(photo_exif, b'not a valid image payload')))
    invalid_offset = bytearray(with_thumbnail(photo_exif, preview))
    root_offset = struct.unpack_from('<I', invalid_offset, 4)[0]
    root_count = struct.unpack_from('<H', invalid_offset, root_offset)[0]
    thumbnail_ifd = struct.unpack_from('<I', invalid_offset, root_offset + 2 + 12 * root_count)[0]
    struct.pack_into('<I', invalid_offset, thumbnail_ifd + 2 + 12 + 8, 0xfffffff0)
    png('refused-preview.png', chunk('eXIf', invalid_offset))
    png('text-preview.png', chunk('tEXt', b'PreviewImage\0This is text, not a binary image resource'))
    png('pretend-preview.png', chunk('tEXt', b'PreviewImage\0(Binary data 25 bytes, use -b option to extract)'))

# Format coverage for 1.0

Identification and metadata extraction are different from pixel/audio/video decoding or complete structural validation. All inputs are read as data; executables, scripts and macros are never launched as inspected payloads.

| Input | Native identification | Metadata evidence exercised | Native deep validation |
| --- | --- | --- | --- |
| PNG | PNG signature | Generated text/iTXt/zTXt/EXIF, duplicates, generation records; real C2PA sample | Chunk framing, selected ordering/IHDR rules, CRCs, blocks, trailing bytes; no compressed-pixel/APNG semantic validation |
| JPEG | JPEG marker signature | Generated EXIF/thumbnail and real signed camera sample | Marker/segment framing, blocks, trailing bytes; no entropy/pixel correctness claim |
| TIFF | TIFF/BigTIFF signature | Generated camera IFDs and upstream TIFF camera tags | None |
| WebP | RIFF + WEBP | Upstream extended WebP metadata fixture, width | None |
| GIF | GIF87a/89a | Upstream GIF/XMP title fixture | None |
| BMP | BM | Upstream BMP header metadata fixture | None |
| HEIC/HEIF | HEIF brand container | Upstream HEIF metadata fixture, width | None; no native HEIF pixel decoder |
| MP4 | ISO BMFF brand container | Generated metadata-only ISO BMFF/QuickTime fixture | None; playable video decoding not tested |
| MOV | QuickTime container/candidate | Upstream QuickTime metadata fixture, make/duration | None; playable video decoding not tested |
| MKV | EBML container | Upstream Matroska metadata fixture, width/duration | None; EBML alone is not a proven Matroska subtype |
| MP3 | ID3 or MPEG audio candidate | Upstream MP3 metadata fixture, title | None; ID3 alone does not prove MP3 audio |
| FLAC | fLaC | Upstream FLAC metadata fixture, title | None; audio decoding not tested |
| WAV | RIFF + WAVE | Generated PCM container, channel metadata | None |
| PDF | %PDF- | Generated document author | None; PDF digital signatures not validated |
| DOCX | ZIP container | Generated OOXML creator property | None; ZIP signature alone does not prove DOCX |
| ZIP | ZIP container | Generated archive metadata | None; archive content not executed or exhaustively expanded |
| PE | DOS header plus PE signature | The built test executable inspected through ExifTool | None; Authenticode not validated |

Upstream metadata fixtures come from [ExifTool 13.59's test corpus](https://github.com/exiftool/exiftool/tree/2200871d9cef988051d2a99d67df3bda6cbb30a8/t/images). Exact URLs and SHA-256 digests are recorded in tests/format-fixtures.json and fetched only by explicit bootstrap. Many are compact metadata test files, not complete renderable/playable assets. They are not redistributed in the source ZIP. Test assertions check the file type and a concrete metadata property, plus unchanged bytes. This breadth is metadata coverage, not an exhaustive compatibility guarantee.

The automated formats suite passed 82 assertions for this candidate: all listed native/container labels, concrete parser fields and unchanged fixture hashes. No fixture reported an analysis finding in that run. PNG/JPEG malformed, Unicode, duplicate, huge, empty, extraction and provenance cases have separate suites. Embedded-preview rendering uses only Microsoft PNG/JPEG/GIF/BMP/TIFF codecs, first frame only, with compressed-byte and pixel budgets; it does not add general-purpose image playback.

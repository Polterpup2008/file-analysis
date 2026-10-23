# File Analysis 0.6.0 development milestone

Embedded preview recovery and a native image viewer are being validated toward 1.0. Supported ExifTool binary tags are ThumbnailImage, PreviewImage, JpgFromRaw and OtherImage. Exact recovered bytes have source fields and SHA-256 hashes; no byte offset is invented. JSON/CSV/text reports retain resource summaries, while payloads are exported separately to new files. Share summaries exclude embedded image bytes.

Recovery is bounded to eight resources and 8 MiB of compressed payloads per file. Viewing uses Microsoft Windows Imaging Component codecs for PNG/JPEG/GIF/BMP/TIFF, first frame only, with a 16,777,216-pixel / 16,384-per-axis budget. Larger or damaged payloads can remain available for raw extraction. Only one decoded viewer is retained at a time. Preview bytes are included in the batch memory budget. Extracted images can retain personal metadata and do not establish that they match the current file's visible content.

"Exact recovered bytes" means the binary reader output, not a claimed source-file byte span: ExifTool can normalize some vendor preview encodings. Strict-reader warnings are retained when relaxed recovery is attempted. Decoder availability and recovery are separate results. Missing or unreadable preview records do not establish absence.

Six local suites pass (301 assertions, including 47 preview checks). Tests cover actual ExifTool extraction from generated PNG/JPEG fixtures, malformed/refused/unsupported payloads, forged text tags, binary/base64 integrity, declared dimension budgets, safe destinations, cancellation and unchanged originals. The actual Win32 app displayed the generated gradient thumbnail. The row label was shortened after observing clipping and the viewer title initialization was corrected. Standard-dialog extraction and the revised title still require a final interactive check; exact extracted bytes are verified by the automated suite.

This is a local development checkpoint toward 1.0, not a published release.

# 1.0 requirement/evidence audit — in progress

Candidate status: **not yet approved as a completed 1.0 release**. Each claim below requires evidence beyond a version bump. Public release 0.5 remains the last verified published download at the start of this audit.

Local baseline: ten suites passed 436 assertions; a subsequent ordinary-prompt/invalid-ComfyUI-node correction passed the engine, presentation and generation suites with two additional checks. The clean-source packaging run below must verify the combined candidate. The owned 0.8 test process with two generated files used 23.6 MiB working set / 4 MiB private memory and zero measured CPU time over a 5.15-second idle sample; this is a small-input observation, not a worst-case benchmark.

| Requirement | State | Evidence and remaining limits |
| --- | --- | --- |
| Native C++20 / Win32 / MSVC / x64 / offline UI | VERIFIED | Actual executable built and launched on Windows x64; Common Controls, no service/web UI; bundled parsers with explicit offline settings. Windows 10 compatibility target is compiled, not separately exercised on a Windows 10 machine. |
| Streamed hashes, filesystem timestamps, signature mismatches | VERIFIED | Engine suite: known SHA-256 vectors, >4 GiB cancellation, Unicode, mismatched extensions, inaccessible/missing files. SHA-1 labeled legacy. |
| Namespace/duplicate/raw extraction and honest locations | VERIFIED | Real ExifTool text/numeric calls, duplicate fields and source grouping; exact native offsets only. Reader retention/output limits are explicit. |
| Camera/lens/settings/time/GPS conflicts and absence | VERIFIED | 61 photo checks including generated EXIF/XMP, conflicts, zero GPS, unavailable reader, unchanged originals and optional local Canon sample. Stored claims do not identify a physical device or person. |
| Origin, privacy review and filtered share exports | VERIFIED | 39 origin/privacy checks; actual 0.5 dialog/JSON export. Selection omits groups/raw payloads, not guaranteed anonymization. |
| Generation conventions and exports | VERIFIED | Existing ComfyUI/A1111-compatible checks plus 25 InvokeAI/Fooocus/visible NovelAI checks. No unverified convention, steganography, identity or watermark detection claim. |
| C2PA claims, relationships, integrity and trust separation | VERIFIED | 41 provenance checks with an official signed file and modified copy; earlier private Gemini regression: 2 manifests/344 chunks. Pinned offline trust snapshot; no current revocation guarantee. |
| Native structure, checksums, raw bytes, hex and extraction | VERIFIED | PNG/JPEG framing/CRC/trailing/unknown-block tests, exact raw-block extraction, safe new destinations, hard-link protection. No full compressed-stream validation claim. |
| Embedded previews and raster bounds | PARTIAL | 47 automated checks pass and the native viewer displayed the generated thumbnail. Title initialization fixed after inspection; revised title and Save-dialog extraction still need interactive verification. |
| Batch search / navigation | PARTIAL | 17 checks pass for Unicode, nested raw values, limits, cancellation and no evidence mutation. Actual GUI Find/navigation still pending. |
| Readable comparison in both views | PARTIAL | 10 checks pass for left/right values, duplicates, blocks, limits, cancellation and unchanged exports. Actual GUI comparison still pending. |
| Representative format metadata coverage | VERIFIED | The formats suite passed 82 assertions: 17 input families, actual parser properties and unchanged bytes. See FORMAT-COVERAGE.md for exact container labels and metadata-only fixture quality. |
| GUI open/copy/export/filter/resize/batch/cancel paths | PARTIAL | 0.5 Simple/Detailed and share export were exercised; preview viewing exercised in 0.6. A separate nsys.exe Windows Security prompt blocks current desktop checks; no security prompt is automated. |
| Repeatable packaging / source agreement / hosted builds | PARTIAL | Pinned bootstrap and PowerShell syntax checks pass. New packaging pipeline, fresh staging and GitHub workflow still require actual runs and artifact verification. Bit-identical builds across toolchains are not claimed. |
| Public executable/source/checksum publication | NOT TESTED | Pending final checks and source-tree/asset digest agreement. |

No placeholders were added for unsupported readers or validators. Remaining acceptance work is explicit. These states must be updated from observed test/runtime/publication results before declaring the active 1.0 goal complete.

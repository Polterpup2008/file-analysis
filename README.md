# File Analysis

A lightweight Windows desktop app for exploring what a file actually contains: hidden metadata, AI generation settings, Content Credentials, editing claims, and unusual binary structures.

Written in C++20 with native Win32 controls. Analysis runs locally and offline by default. There is no service, web UI, telemetry or automatic update check.

## Download and run

Download **FileAnalysis-0.4.0-Windows-x64.zip** from the [latest release](https://github.com/Polterpup2008/file-analysis/releases/latest). Requires Windows 10 or 11, 64-bit.

1. Extract the entire ZIP to a folder.
2. Run `file-analysis.exe` inside it. Keep the adjacent `exiftool` and `c2patool` folders intact.
3. Choose **Open File**, select a file, and read its findings. Use **View: Detailed** when you want the underlying evidence.

The portable download includes its parser tools and required runtime files. You do not need Visual Studio, Python, or a separate parser installation to use it. Administrator access is not required. The executable is not code-signed.

## What it does

- **Simple and Detailed views:** start with useful findings, then inspect every recovered field, raw value, source and available byte location.
- **Camera and capture details:** surface recorded device manufacturer/model, lens, capture time and timezone, exposure, aperture, ISO, focal length, flash, shooting modes, device/lens serial numbers and GPS metadata. Flag different recorded device identities and retain every source.
- **AI metadata:** recover ComfyUI workflows/API prompt JSON and AUTOMATIC1111-compatible prompts and settings when present. Surface explicit AI claims in C2PA provenance separately from generation parameters.
- **Content Credentials:** reconstruct linked provenance records, show recorded creation/editing/watermark actions, and independently check active-manifest integrity and certificate trust using the bundled official c2patool.
- **File forensics:** identify formats by signatures, calculate SHA-256/SHA-1 hashes, inspect PNG chunks and JPEG segments, check PNG CRCs, flag unknown blocks and reliably identifiable trailing data, and view or extract raw blocks.
- **Metadata extraction:** use ExifTool to read EXIF, XMP, IPTC and other available format-specific information, preserving duplicate fields and keeping scanner diagnostics separate.
- **Comparison and reports:** compare two analyzed files; export full JSON, CSV or text reports; optionally redact local file paths before sharing.

Native deep structure inspection covers **PNG and JPEG**. Other recognized formats receive signature identification, hashes, filesystem details and metadata available through ExifTool; this does not imply full structural validation of every format.

This app reads recorded evidence. It is **not a visual AI detector**: missing metadata does not prove a file is human-made. A recorded SynthID action is not an independent watermark test. A valid signature and a trusted certificate do not prove the factual truth of every action or the identity of a human creator.

Version **0.4.0** is an early release. Its supported checks and limitations are described below.

## Use

- **Open File** accepts multiple files and immediately analyzes them. **Open Folder** selects a directory; press **Scan**. Subfolder scanning skips reparse points and junctions.
- **View: Simple / Detailed** controls how much is shown. **Simple** is the default: AI creation claims, recorded watermark actions, readable provenance history, separate integrity/trust checks, recovered generation settings, selected creator/software/comment/location fields, and unusual structures. Routine properties, hashes, parser diagnostics, raw JSON and inspection controls stay in **Detailed**. Errors remain visible in Simple; absent clues and incomplete scans are labeled explicitly.
- Select a Simple finding for a plain-language explanation and its limits. Simple details wrap to the window. **Detailed** restores every field and the **Section** selector: Overview, Metadata, Structure, Hex, Comparison and Provenance. Switching views does not rescan or change evidence. It clears the search/category filters; exports always contain the full analysis regardless of view.
- **Detailed > Provenance** shows the manifests in source-link order, ingredient links and independent validation results. Record numbers in Simple are reading labels; disconnected records do not establish a chronology. Select any row for its evidence and limitations.
- **Metadata** retains individual parser entries and duplicate groups. `ExifTool:*` entries are **Analysis diagnostics**, never EXIF. Composite fields are **Derived values**. Scanner-generated values are excluded from comparison.
- **Structure** groups consecutive IDAT chunks by default. Uncheck **Group repeated IDAT chunks** for individual offsets, hashes, CRC results, hex inspection and **Extract block**. Grouping does not remove any raw block from the JSON export. Double-click an individual block to open its offset in **Hex**.
- Search and category filters apply to the selected view. **Copy** or Ctrl+C copies selected table rows, including the complete record for one selection. Select exactly two files with Ctrl+click and use **Compare two** for observed differences.
- **Export Results** supports JSON, CSV and UTF-8 plain text (an indented, complete report). **Redact local paths in exports** is checked by default. Uncheck it for an exact report. The generation export uses the same option. Raw block extraction always preserves exact bytes.
- Path redaction covers textual local Windows paths, source filenames, parser output, errors and comparison data. It is **not anonymization**: embedded authors, identifiers, public URLs and binary/hex/base64 payloads can contain personal data. Review a report before sharing.
- Exports and extraction require a **new destination file**. Existing files and evidence aliases/hard links are protected. Cancellation during writing can leave an incomplete destination; the operation reports this explicitly.

## Camera and capture metadata (new in 0.4)

Simple view now shows a **Camera / device reported** finding for images. If the file contains a device model, it displays the exact stored name or model code. If only a manufacturer is present, it says the model was not recovered. If no make/model was recovered, it says so; a disabled or unsuccessful metadata reader is labeled **Not evaluated** instead.

When available, additional findings show the lens, embedded capture date and original timezone offset, exposure/aperture/ISO/focal length, shooting modes, serial numbers and recorded GPS coordinates. Settings are grouped to keep Simple readable. Select a finding for its exact source tags and interpretation limits. **Detailed > Metadata**, category **Photo / device**, contains the summary with source/numeric evidence. The original EXIF/XMP/MakerNote fields are also retained.

The summary recognizes standard EXIF and the relevant XMP camera namespaces, selected MakerNote identity tags, and QuickTime Apple/Android make/model fields. It operates on metadata returned by ExifTool, including supported HEIF and camera RAW files; this adds no native RAW or HEIF pixel decoder. Camera claims in ICC profiles, scanner diagnostics, generation settings, thumbnails and separately embedded documents are excluded from the device summary. Different recorded device identities are shown together with an explicit disagreement finding rather than choosing one as fact.

Camera tags can be edited, forged or carried over from another image. They do not prove that the current file was photographed, which physical device produced it, or who took it. Internal device codes are not expanded to guessed retail names. No camera is inferred from pixels, filenames, editor names or a lens alone. Absent metadata does not establish AI generation. Capture timestamps are not filesystem timestamps; an absent timezone is never silently treated as UTC. GPS is displayed without contacting a map service.

**Local-path redaction does not remove GPS coordinates or serial numbers.** Reports preserve this evidence; review it before sharing. JSON schema version 3 adds the `files[].photo` object (status, properties, original source fields, numeric values, disagreements and limits). CSV and text retain the camera summary and source evidence too.

## C2PA interpretation and verification

`caBX` is recognized as the C2PA JUMBF manifest-store chunk. Its CRC validates PNG chunk bytes, not C2PA signatures. Native and ExifTool classifications share the format registry in `src/format_registry.hpp`.

The bundled official `c2patool` 0.28.2 independently validates recovered embedded C2PA evidence. Its ordinary JSON report is used; `--info` is a statistics command and does not use the configured validation context in this version. The original file remains open for read with writes/deletion disallowed during analysis.

The report separates:

1. **Claims:** recorded creators/software, action descriptions, AI digital-source types, ingredient relationships and historical embedded validation records. Google/SynthID names are not hardcoded as proof of origin. A SynthID action description is not an independent watermark test.
2. **Active manifest integrity:** independently checked claim signature, assertion results and current-file content binding. A valid claim signature with a failed file hash is reported as failed manifest integrity.
3. **Certificate trust:** evaluated against the bundled C2PA Conformance Trust List snapshot dated 2026-10-22. This can be untrusted even when signature mathematics and content binding are valid. Timestamp results have separate properties in the full validation record.

The earlier manifests' embedded validation records remain labeled as historical claims. Independently returned ingredient validation deltas are retained separately; they are not presented as a fresh full verification of every historical signature. Parent links establish a partial history, not a complete editing history. Cycles are reported; absent active-manifest information is not guessed.

No model, prompt, seed or account is inferred from C2PA software or author names. Explicit trained-algorithm C2PA actions are surfaced in **AI provenance**, independently of ComfyUI/AUTOMATIC1111 **generation parameters**. Missing parameters never negate recovered AI provenance. Filesystem timestamps are not generation timestamps; a downloaded/copied file can have a local creation time later than its modification time.

## Offline operation and limits

The pinned offline settings disable remote manifests, network OCSP and identity-assertion decoding, and use an empty SDK network host allowlist. Validation uses embedded material and local trust anchors. Binary, settings and trust-list fingerprints are checked before execution; replacing them requires an intentional source/dependency update. There is no online freshness/revocation guarantee and no automatic trust-list refresh.

External processes run without a shell, with an inherited-handle allowlist, a sanitized environment, a 20-second timeout, a 512 MiB process memory limit, at most four child processes and a kill-on-close Job Object. Cancellation terminates the parser job. Output is limited to 16 MiB per stream; JSON parsing is limited to depth 128 and 250,000 parser events. Interpretation is limited to 256 C2PA manifests. Limit failures remain visible and exported.

Native analysis uses streamed SHA-256/SHA-1 and 64-bit offsets. Native structure checks cover PNG framing, CRCs, selected core ordering/IHDR rules and JPEG marker/segment framing. PNG compressed pixels/APNG semantics and JPEG compressed image correctness are not decoded. The native structure budget is 30 seconds / 20,000 blocks; hashing has a 120-second budget. PNG native text retention is 1 MiB per chunk / 4 MiB total. A batch is bounded to 256 files, directory depth 32 and roughly 64 MiB of serialized results plus the current result and export copies.

Other formats receive signature identification, filesystem information, hashes and metadata returned by ExifTool; native deep structural validation is not claimed for them. Binary payload extraction, all proprietary metadata, remote-only manifests, exhaustive embedded-media extraction, InvokeAI/Fooocus/NovelAI-specific parameter decoding, Authenticode and PDF signature verification are not implemented. Signature/trust checks do not establish the factual truth of an action or a person's identity.

## Build and tests

Requires Windows 10/11 x64, Visual Studio 2022 C++ tools + Windows SDK, CMake 3.24+, and Python 3 for fixture generation. The packaged application does not require Python or a separate VC runtime installation.

From a source checkout, run `scripts/bootstrap-dependencies.ps1` once with network access; downloads are pinned by SHA-256. It also fetches the official signed test fixture. Normal builds and tests operate offline.

```powershell
.\scripts\bootstrap-dependencies.ps1
python tests/make_fixtures.py
cmake -S . -B build-v04 -G "Visual Studio 17 2022" -A x64
cmake --build build-v04 --config Release -j 4
ctest --test-dir build-v04 -C Release --output-on-failure
cmake --install build-v04 --config Release --prefix dist/FileAnalysis-0.4.0
```

Alternatively run `scripts/build.ps1`; it can locate CMake bundled with Visual Studio when CMake is not on PATH. `-Package` additionally installs the portable distribution.

The engine suite covers valid/empty/malformed data, unknown formats, duplicates, Unicode, >4 GiB size handling, interrupted scans, inaccessible files, parser timeouts, hostile environment hooks, exports, hard-link protection and unchanged original bytes. The provenance suite uses a real signed asset and a modified copy to distinguish signature validity, content binding and trust; it also covers fallback extraction, historical validation claims, cycles, grouped chunks, malformed C2PA, diagnostic comparison and privacy redaction. The presentation suite checks information selection, honest empty/incomplete states, visible corruption, generation settings, source-link history and separation of claims, integrity and trust. The photo suite uses synthetic JPEG/PNG/TIFF EXIF and XMP-only/conflicting metadata, checks settings/timezone/GPS/serials, rejects unrelated model fields, distinguishes absent from unavailable metadata, checks all export formats and verifies unchanged original bytes. It also exercises a real Canon sample when present. The GUI driver in `tests/gui_smoke.mjs` uses actual controls through the Computer Use API; it is separate from CTest and requires an interactive desktop.

For a private local PNG regression, pass its path as the optional fourth argument to `provenance-tests.exe` after the fixture directory, ExifTool path and c2patool path. That regression checks the specific two-manifest/344-chunk Gemini case and writes path-redacted reports under `tests/output`. The private image and its old export are not bundled as fixtures.

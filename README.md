# File Analysis

A lightweight Windows desktop app for exploring what a file actually contains: hidden metadata, AI generation settings, Content Credentials, editing claims, and unusual binary structures.

Written in C++20 with native Win32 controls. Analysis runs locally and offline by default. There is no service, web UI, telemetry or automatic update check.

## Download and run

Download the **FileAnalysis-<version>-Windows-x64.zip** asset from the [latest release](https://github.com/Polterpup2008/file-analysis/releases/latest). Requires Windows 10 or 11, 64-bit.

1. Choose the **Windows-x64 ZIP**, then extract it to a folder. GitHub’s **Source code** archives contain source files and require a build.
2. Run `file-analysis.exe` inside it. Keep the adjacent `exiftool` and `c2patool` folders intact.
3. Choose **Open File**, select a file, and read its findings. Use **View: Detailed** when you want the underlying evidence.

The portable download includes its parser tools and required runtime files. You do not need Visual Studio, Python, or a separate parser installation to use it. Administrator access is not required. The executable is not code-signed.

## What it does

- **Simple and Detailed views:** start with useful findings, then inspect every recovered field, raw value, source and available byte location.
- **Camera and capture details:** surface recorded device manufacturer/model, lens, capture time and timezone, exposure, aperture, ISO, focal length, flash, shooting modes, device/lens serial numbers and GPS metadata. Flag different recorded device identities and retain every source.
- **Origin and privacy summaries:** bring camera, AI provenance, generation settings and software claims together; highlight location, identifiers, names/contact/rights claims, preview tags and free text to review.
- **Share reports:** choose which sensitive field groups to omit from a compact JSON/CSV/text summary. Raw payloads and unstructured text are excluded; the full local analysis stays intact.
- **AI metadata:** recover ComfyUI workflows/API prompt JSON, AUTOMATIC1111-compatible settings (including compatible Forge exports), named InvokeAI JSON, marker-qualified Fooocus JSON and visible NovelAI JSON when present. Surface explicit AI claims in C2PA provenance separately from generation parameters.
- **Content Credentials:** reconstruct linked provenance records, show recorded creation/editing/watermark actions, and independently check active-manifest integrity and certificate trust using the bundled official c2patool.
- **File forensics:** identify formats by signatures, calculate SHA-256/SHA-1 hashes, inspect PNG chunks and JPEG segments, check PNG CRCs, flag unknown blocks and reliably identifiable trailing data, and view or extract raw blocks.
- **Metadata extraction:** use ExifTool to read EXIF, XMP, IPTC and other available format-specific information, preserving duplicate fields and keeping scanner diagnostics separate.
- **Embedded previews:** recover supported thumbnail/preview payloads with source fields and SHA-256, view bounded raster previews in a native window and extract the recovered bytes to a new file.
- **Batch search:** search full retained evidence across analyzed files and navigate directly to a matching field without rescanning.
- **Comparison and reports:** show added/removed/changed evidence with left/right values for two analyzed files; export full JSON, CSV or text reports; optionally redact local file paths before sharing.

Native deep structure inspection covers **PNG and JPEG**. Other recognized formats receive signature identification, hashes, filesystem details and metadata available through ExifTool; this does not imply full structural validation of every format.

This app reads recorded evidence. It is **not a visual AI detector**: missing metadata does not prove a file is human-made. A recorded SynthID action is not an independent watermark test. A valid signature and a trusted certificate do not prove the factual truth of every action or the identity of a human creator.

The **1.0 candidate** adds preview recovery, batch search, readable comparison and additional generation conventions. Final desktop/release checks are tracked in [the acceptance audit](docs/AUDIT-1.0.md). The latest verified public release is still 0.5 until that audit is complete.

## Use

- **Open File** accepts multiple files and immediately analyzes them. **Open Folder** selects a directory; press **Scan**. Subfolder scanning skips reparse points and junctions.
- **View: Simple / Detailed** controls how much is shown. **Simple** is the default: AI creation claims, recorded watermark actions, readable provenance history, separate integrity/trust checks, recovered generation settings, selected creator/software/comment/location fields, and unusual structures. Routine properties, hashes, parser diagnostics, raw JSON and inspection controls stay in **Detailed**. Errors remain visible in Simple; absent clues and incomplete scans are labeled explicitly.
- Select a Simple finding for a plain-language explanation and its limits. Simple details wrap to the window. **Detailed** restores every field and the **Section** selector: Overview, Metadata, Structure, Hex, Comparison, Provenance and Resources. Switching views does not rescan or change evidence. It clears the search/category filters; exports always contain the full analysis regardless of view.
- **Detailed > Provenance** shows the manifests in source-link order, ingredient links and independent validation results. Record numbers in Simple are reading labels; disconnected records do not establish a chronology. Select any row for its evidence and limitations.
- **Metadata** retains individual parser entries and duplicate groups. `ExifTool:*` entries are **Analysis diagnostics**, never EXIF. Composite fields are **Derived values**. Scanner-generated values are excluded from comparison.
- **Structure** groups consecutive IDAT chunks by default. Uncheck **Group repeated IDAT chunks** for individual offsets, hashes, CRC results, hex inspection and **Extract block**. Grouping does not remove any raw block from the JSON export. Double-click an individual block to open its offset in **Hex**.
- Local search/category controls filter the displayed rows and compact summaries. For full retained values, check **Search all files**, enter a query, optionally select a category, and press **Find**. It also works with a one-file batch. **Cancel** retains partial matches; a 5000-row limit is explicit. **Open match** or double-click navigates to its source field. Changing query/category clears the old matches.
- **Copy** or Ctrl+C copies selected rows, including the complete record for one selection. **Compare two** automatically uses the pair when exactly two files are analyzed; larger batches require selecting two with Ctrl+click. Read Added/Removed/Changed rows with left/right values and select a row for original sources, duplicates and raw evidence. Raw blocks compare corresponding indices, not inferred moves. **Back to file** returns to findings. Full comparison evidence remains in exports.
- **Export Results** supports JSON, CSV and UTF-8 plain text (an indented, complete report). **Redact local paths in exports** is checked by default. Uncheck it for an exact report. The generation export uses the same option. Raw block extraction always preserves exact bytes.
- Path redaction covers textual local Windows paths, source filenames, parser output, errors and comparison data. It is **not anonymization**: embedded authors, identifiers, public URLs and binary/hex/base64 payloads can contain personal data. Review a report before sharing.
- Exports and extraction require a **new destination file**. Existing files and evidence aliases/hard links are protected. Cancellation during writing can leave an incomplete destination; the operation reports this explicitly.

## Origin, privacy and sharing

**Recorded origin signals** summarizes the metadata recovered from the file. Camera tags, AI involvement recorded in C2PA and embedded generation parameters remain distinct signals and may coexist. An editor name is a software claim, not proof of an editing action. Missing metadata gives **Origin not established**. No visual detector, device guess or complete-history reconstruction is added.

**Review before sharing** highlights location/GPS, device/document identifiers, names/contact/rights claims, embedded preview tags and free text. Select it to inspect source fields. Detailed > Metadata contains the **Origin summary** and **Privacy findings** categories. The privacy review recognizes selected metadata fields; it does not exhaustively inspect arbitrary binary data or detect every personal detail.

**Share report...** opens a native dialog with independent controls to omit location/GPS, device/document identifiers and names/contact/rights fields. All three are selected by default. Save the result as JSON, CSV or text to a new file. This creates a compact report: it always excludes source paths/filenames, raw parser objects, binary blocks, full provenance manifests, prompts, workflows, free text and raw error messages. Files are labeled File 1, File 2, etc. Analysis/batch error counts and cancellation status remain explicit. SHA-256, retained capture settings and software claims remain available.

This is **not guaranteed anonymization**. Retained dates, camera/lens/software values and file fingerprints can identify a file or contain personal information. Review the result before sharing. Share report changes the exported summary only; originals and the complete in-memory analysis remain intact. **Export Results...** still produces the full forensic report, with its existing optional local-path redaction.

JSON schema 5 retains `files[].origin_summary`, `files[].privacy`, capture metadata and `files[].embedded_previews` resource summaries. Payload bytes are extracted separately, not embedded in reports. Share reports are explicitly labeled `report_mode: share_summary`, carry the omission policy and limitations, and contain a deliberately smaller set of evidence. They are not interchangeable with full forensic reports.

## Camera and capture metadata

Simple view now shows a **Camera / device reported** finding for images. If the file contains a device model, it displays the exact stored name or model code. If only a manufacturer is present, it says the model was not recovered. If no make/model was recovered, it says so; a disabled or unsuccessful metadata reader is labeled **Not evaluated** instead.

When available, additional findings show the lens, embedded capture date and original timezone offset, exposure/aperture/ISO/focal length, shooting modes, serial numbers and recorded GPS coordinates. Settings are grouped to keep Simple readable. Select a finding for its exact source tags and interpretation limits. **Detailed > Metadata**, category **Photo / device**, contains the summary with source/numeric evidence. The original EXIF/XMP/MakerNote fields are also retained.

The summary recognizes standard EXIF and the relevant XMP camera namespaces, selected MakerNote identity tags, and QuickTime Apple/Android make/model fields. It operates on metadata returned by ExifTool, including supported HEIF and camera RAW files; this adds no native RAW or HEIF pixel decoder. Camera claims in ICC profiles, scanner diagnostics, generation settings, thumbnails and separately embedded documents are excluded from the device summary. Different recorded device identities are shown together with an explicit disagreement finding rather than choosing one as fact.

Camera tags can be edited, forged or carried over from another image. They do not prove that the current file was photographed, which physical device produced it, or who took it. Internal device codes are not expanded to guessed retail names. No camera is inferred from pixels, filenames, editor names or a lens alone. Absent metadata does not establish AI generation. Capture timestamps are not filesystem timestamps; an absent timezone is never silently treated as UTC. GPS is displayed without contacting a map service.

**Local-path redaction does not remove GPS coordinates or serial numbers.** Reports preserve this evidence; review it before sharing. The `files[].photo` object retains status, properties, original source fields, numeric values, disagreements and limits. CSV and text retain the camera summary and source evidence too.

## Embedded previews and generation records

Select an **Embedded preview** finding in Simple or **Detailed > Resources**. **View preview** opens the first frame of a recovered PNG/JPEG/GIF/BMP/TIFF raster using Microsoft codecs. **Extract preview** writes its cached reader output to a new file. Full details retain the source field, size, SHA-256, dimensions and warnings; source offsets remain unavailable when the reader supplies none. Some vendor encodings can be normalized by ExifTool, so recovered bytes are not claimed as an original source-file byte span.

Recovery is limited to eight supported resources and 8 MiB per file. Rendering is limited to 16,777,216 pixels and 16,384 per axis, with one decoded viewer at a time. Corrupt/unsupported/oversized payloads can remain extractable with warnings. No preview tags, failed recovery and an unavailable reader are distinct states. Extracted images may retain personal metadata and need not match the current file's visible content.

**Export generation** in Detailed writes the recovered records with their source fields; workflow/parameter objects are under `generation[].value`. No record is executed or imported. InvokeAI recognition uses named JSON envelopes and supported object shapes. Fooocus requires an explicit scheme/software marker. NovelAI recognition covers visible marker-qualified JSON, not hidden pixel metadata or vendor signature verification. JSON recognition is bounded to 1 MiB, depth 64 and 20000 nodes, with at most 1024 interpreted entries. Full source metadata retains unknown or unsupported values.

## C2PA interpretation and verification

`caBX` is recognized as the C2PA JUMBF manifest-store chunk. Its CRC validates PNG chunk bytes, not C2PA signatures. Native and ExifTool classifications share the format registry in `src/format_registry.hpp`.

The bundled official `c2patool` 0.28.2 independently validates recovered embedded C2PA evidence. Its ordinary JSON report is used; `--info` is a statistics command and does not use the configured validation context in this version. The original file remains open for read with writes/deletion disallowed during analysis.

The report separates:

1. **Claims:** recorded creators/software, action descriptions, AI digital-source types, ingredient relationships and historical embedded validation records. Google/SynthID names are not hardcoded as proof of origin. A SynthID action description is not an independent watermark test.
2. **Active manifest integrity:** independently checked claim signature, assertion results and current-file content binding. A valid claim signature with a failed file hash is reported as failed manifest integrity.
3. **Certificate trust:** evaluated against the bundled C2PA Conformance Trust List snapshot dated 2026-10-22. This can be untrusted even when signature mathematics and content binding are valid. Timestamp results have separate properties in the full validation record.

The earlier manifests' embedded validation records remain labeled as historical claims. Independently returned ingredient validation deltas are retained separately; they are not presented as a fresh full verification of every historical signature. Parent links establish a partial history, not a complete editing history. Cycles are reported; absent active-manifest information is not guessed.

No model, prompt, seed or account is inferred from C2PA software or author names. Explicit trained-algorithm C2PA actions are surfaced in **AI provenance**, independently of **generation parameters** from every implemented convention. Missing parameters never negate recovered AI provenance. Filesystem timestamps are not generation timestamps; a downloaded/copied file can have a local creation time later than its modification time.

## Offline operation and limits

The pinned offline settings disable remote manifests, network OCSP and identity-assertion decoding, and use an empty SDK network host allowlist. Validation uses embedded material and local trust anchors. Binary, settings and trust-list fingerprints are checked before execution; replacing them requires an intentional source/dependency update. There is no online freshness/revocation guarantee and no automatic trust-list refresh.

External processes run without a shell, with an inherited-handle allowlist, a sanitized environment, a 20-second timeout, a 512 MiB process memory limit, at most four child processes and a kill-on-close Job Object. Cancellation terminates the parser job. Output is limited to 16 MiB per stream; JSON parsing is limited to depth 128 and 250,000 parser events. Interpretation is limited to 256 C2PA manifests. Limit failures remain visible and exported.

Native analysis uses streamed SHA-256/SHA-1 and 64-bit offsets. Native structure checks cover PNG framing, CRCs, selected core ordering/IHDR rules and JPEG marker/segment framing. PNG compressed pixels/APNG semantics and JPEG compressed image correctness are not decoded. The native structure budget is 30 seconds / 20,000 blocks; hashing has a 120-second budget. PNG native text retention is 1 MiB per chunk / 4 MiB total. A batch is bounded to 256 files, directory depth 32 and roughly 64 MiB of serialized results plus retained preview bytes, the current result and export copies.

Other formats receive signature identification, filesystem information, hashes and metadata returned by ExifTool; native deep structural validation is not claimed for them. Arbitrary binary tag payloads, all proprietary metadata, remote-only manifests, exhaustive embedded-media extraction, legacy/undocumented generator variants, NovelAI alpha-channel extraction/vendor signatures, Authenticode and PDF signature verification are not implemented. See the [format coverage matrix](docs/FORMAT-COVERAGE.md) for tested metadata and validation scope. Signature/trust checks do not establish the factual truth of an action or a person's identity.

## Build and tests

Requires Windows 10/11 x64, Visual Studio 2022 C++ tools + Windows SDK, CMake 3.24+, and Python 3 for fixture generation. The packaged application does not require Python or a separate VC runtime installation.

From a source checkout, run `scripts/bootstrap-dependencies.ps1` once with network access; downloads are pinned by SHA-256. It also fetches the official signed test fixture. Normal builds and tests operate offline.

```powershell
.\scripts\bootstrap-dependencies.ps1
python tests/make_fixtures.py
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist/FileAnalysis-1.0.0
```

Alternatively run `scripts/build.ps1`; it can locate CMake bundled with Visual Studio when CMake is not on PATH. `-Package` additionally installs the portable distribution. From a clean Git checkout, **scripts/package.ps1** runs the build/tests, installs fresh staging and creates Windows/source ZIPs, BUILD.txt and SHA-256 checksums. It refuses existing ZIP destinations; choose a fresh output directory when repackaging. The GitHub Windows workflow runs the same pipeline and retains downloadable artifacts; it does not publish or overwrite public releases automatically. Repeatable commands do not promise bit-identical output across different MSVC/SDK/runner versions.

The engine suite covers valid/empty/malformed data, unknown formats, duplicates, Unicode, >4 GiB size handling, interrupted scans, inaccessible files, parser timeouts, hostile environment hooks, exports, hard-link protection and unchanged original bytes. The provenance suite uses a real signed asset and a modified copy to distinguish signature validity, content binding and trust; it also covers fallback extraction, historical validation claims, cycles, grouped chunks, malformed C2PA, diagnostic comparison and privacy redaction. The presentation suite checks information selection, honest empty/incomplete states, visible corruption, generation settings, source-link history and separation of claims, integrity and trust. The photo suite uses synthetic JPEG/PNG/TIFF EXIF and XMP-only/conflicting metadata, checks settings/timezone/GPS/serials, rejects unrelated model fields, distinguishes absent from unavailable metadata, checks all export formats and verifies unchanged original bytes. It also exercises a real Canon sample when present. The origin/privacy suite checks combined signals, source-linked privacy findings, independent omission groups, removal of duplicated raw payloads, original-evidence preservation, all share export formats and explicit batch/error/cancellation state. The preview suite checks actual binary recovery, native decoding/pixel budgets and safe extraction. Batch-search/comparison suites verify Unicode, nested/duplicate evidence, limits and observed differences. The additional generation suite tests the documented JSON conventions. The format suite checks pinned public metadata fixtures and generated containers without claiming complete media playback. The GUI driver in `tests/gui_smoke.mjs` uses actual controls through the Computer Use API; it is separate from CTest and requires an interactive desktop.

For a private local PNG regression, pass its path as the optional fourth argument to `provenance-tests.exe` after the fixture directory, ExifTool path and c2patool path. That regression checks the specific two-manifest/344-chunk Gemini case and writes path-redacted reports under `tests/output`. The private image and its old export are not bundled as fixtures.

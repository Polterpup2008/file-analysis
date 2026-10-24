# 1.1 camera/phone recognition acceptance audit

This audit records the implementation, tested scope and verified 1.1 delivery on 2026-10-24. Release tag v1.1.0, BUILD.txt and the source ZIP identify source commit 1f75bf854dac9807975cad4644d0dd0a5a64787f. The frozen release archives contain the pre-publication source audit; this main-branch update records the subsequent delivery checks without changing the release bytes.

| Requirement | State | Evidence |
| --- | --- | --- |
| Sourced offline retail-name lookup | VERIFIED | 50,930 factual brand/model/name rows, including 170 Apple hardware identifiers. Pinned input hashes, generation script, MIT notice, exact lookup tests and no runtime network lookup. |
| Camera IDs and DNG model tags | VERIFIED | Allowlisted Canon/Sony/Pentax/Olympus body-ID tags, reader/numeric evidence, unknown-ID states and UniqueCameraModel; real upstream metadata samples and generated decoder fixtures. |
| No fabricated device attribution | VERIFIED | Exact names/codes with manufacturer checks, source pairing, multiple/unknown states, original fields and explicit limits; namespace/co-brand tests keep decoded camera strings without inventing a manufacturer. |
| Bounded/error handling | VERIFIED | Fingerprint-checked immutable catalogue, bounded candidates/sources, large lookup-value limit and visible unavailable/tampered state. Automated missing/altered snapshot checks; actual missing-catalogue desktop fallback. |
| Simple/Detailed and exports | VERIFIED | Readable names beside codes, source/basis detail, full recognition evidence and allowlisted share summaries under schema 6. Actual GUI JSON exports independently inspected. |
| Regression and original-byte preservation | VERIFIED | All eleven suites passed locally (554 assertions) and in hosted run 40087262071 (552 assertions); both include 114 recognition assertions. The local run has two optional Canon photo assertions. Recognition fixtures include ten checksum-pinned upstream metadata samples. Original-byte preservation checks pass. |
| Desktop acceptance | VERIFIED for changed flows | Actual 1.1 Simple/Detailed, friendly/code/source display, Canon/Sony numeric IDs, batch friendly-name search, full/share exports, ambiguous matches and catalogue-unavailable state. See GUI-TESTING.md for observations and limits. |
| Executable/source publication | VERIFIED | PR #4 merged after a green hosted build. All 79 source ZIP files match the canonical committed tree. Fresh extracted Windows ZIP ran with bundled readers/catalogue. Public stable v1.1.0 tag, three asset sizes/digests and a fresh public Windows download SHA-256 match the packaged assets. |

The Android list also includes tablets/TVs/other equipment; it is not a phone-only catalogue. Reader decoding and catalogue matches report retained metadata, not hardware/person authentication. Windows 10 and multi-monitor/high-DPI combinations are not newly claimed as tested by this update.

Release: https://github.com/Polterpup2008/file-analysis/releases/tag/v1.1.0

| Public asset | Bytes | SHA-256 |
| --- | ---: | --- |
| FileAnalysis-1.1.0-Windows-x64.zip | 23,929,706 | 992fc775ea68b65b0bb4c75e83fcd97a24726c6e28de2459189f866aa4a852c6 |
| FileAnalysis-1.1.0-Source.zip | 698,136 | d48e406ef44faee4308ff7ae2444a778ad68a28e13f07bc319b74aed306a2886 |
| SHA256-1.1.0.txt | 197 | e00126bcb7bda442a4e4c3d6946a8ffbfd1ec2f7daeb4fc5bdcc48651489cec8 |

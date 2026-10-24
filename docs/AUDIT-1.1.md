# 1.1 camera/phone recognition acceptance audit

This source audit records the implementation and its tested scope. Publication is a separate gate; the latest stable download remains 1.0 until the 1.1 release assets are verified.

| Requirement | State | Evidence |
| --- | --- | --- |
| Sourced offline retail-name lookup | VERIFIED | 50,930 factual brand/model/name rows, including 170 Apple hardware identifiers. Pinned input hashes, generation script, MIT notice, exact lookup tests and no runtime network lookup. |
| Camera IDs and DNG model tags | VERIFIED | Allowlisted Canon/Sony/Pentax/Olympus body-ID tags, reader/numeric evidence, unknown-ID states and UniqueCameraModel; real upstream metadata samples and generated decoder fixtures. |
| No fabricated device attribution | VERIFIED | Exact names/codes with manufacturer checks, source pairing, multiple/unknown states, original fields and explicit limits; namespace/co-brand tests keep decoded camera strings without inventing a manufacturer. |
| Bounded/error handling | VERIFIED | Fingerprint-checked immutable catalogue, bounded candidates/sources, large lookup-value limit and visible unavailable/tampered state. Automated missing/altered snapshot checks; actual missing-catalogue desktop fallback. |
| Simple/Detailed and exports | VERIFIED | Readable names beside codes, source/basis detail, full recognition evidence and allowlisted share summaries under schema 6. Actual GUI JSON exports independently inspected. |
| Regression and original-byte preservation | VERIFIED locally | All eleven suites passed on 2026-10-24: 554 assertions, including 114 recognition assertions and two optional local Canon photo assertions. Recognition fixtures include ten checksum-pinned upstream metadata samples. Original-byte preservation checks pass. |
| Desktop acceptance | VERIFIED for changed flows | Actual 1.1 Simple/Detailed, friendly/code/source display, Canon/Sony numeric IDs, batch friendly-name search, full/share exports, ambiguous matches and catalogue-unavailable state. See GUI-TESTING.md for observations and limits. |
| Executable/source publication | PENDING | Commit exact sources, green hosted build, canonical-source package, portable execution, release tag and public download/checksum verification. |

The Android list also includes tablets/TVs/other equipment; it is not a phone-only catalogue. Reader decoding and catalogue matches report retained metadata, not hardware/person authentication. Windows 10 and multi-monitor/high-DPI combinations are not newly claimed as tested by this update.

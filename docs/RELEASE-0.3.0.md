# File Analysis 0.3.0 — Windows x64

A portable Windows app for inspecting file metadata, AI generation settings, Content Credentials and binary structure. Its Simple view highlights useful findings; Detailed gives access to the full evidence and inspection tools.

## Download

Download **FileAnalysis-0.3.0-Windows-x64.zip**, extract the entire archive, then run `file-analysis.exe`. Keep the `exiftool` and `c2patool` folders beside it. Requires Windows 10/11 x64. No installer, administrator access, Python or development tools are needed. The app is not code-signed.

## Included

- Native C++20 / Win32 interface with Simple and Detailed views.
- Signature-based format detection, SHA-256/SHA-1 hashes and ExifTool metadata extraction.
- ComfyUI workflow/API prompt JSON and AUTOMATIC1111-compatible generation settings when embedded.
- C2PA creation/editing claims, linked source history, independent active-manifest integrity checks and separate certificate-trust results.
- PNG chunk inspection and CRC checks, JPEG segment framing, hex viewing and raw block extraction.
- File comparison and full JSON, CSV and text exports, with local-path redaction enabled by default.
- Bundled ExifTool 13.59, official c2patool 0.28.2, offline validation settings, trust-list snapshot and third-party notices.

## Limits

This is an early release. Deep native structure checks cover PNG/JPEG, not every ExifTool-supported format. It does not visually detect AI-generated images, independently detect SynthID, infer missing prompts/accounts, or prove human identity. Recorded claims, signature integrity and certificate trust are presented separately. Offline trust uses the bundled 2026-10-22 snapshot, without a current online revocation check.

The three local automated test suites pass. Simple view was visually checked using a real two-manifest Gemini PNG; automation of the final view-switch check was incomplete. Private images and analysis reports are not included in this release.

The accompanying SHA256 file lets you check the portable ZIP with `Get-FileHash -Algorithm SHA256` in PowerShell. Third-party licensing information is included in the archive and repository.

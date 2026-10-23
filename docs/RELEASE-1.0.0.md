# File Analysis 1.0.0 release preparation

This candidate combines Simple/Detailed findings, recorded camera and capture details, origin/privacy summaries, selected-field share reports, independent offline C2PA validation, native PNG/JPEG structure/CRC/hex inspection and safe new-file exports. It adds native embedded-preview viewing/extraction, cancelable batch-wide evidence search, readable two-file differences and documented additional generation-parameter conventions.

The portable Windows x64 package contains file-analysis.exe, both parser companions, runtime files, dependency notices and use instructions. Extract the entire ZIP and run file-analysis.exe; no build tools or administrator rights are required. Source builds use scripts/bootstrap-dependencies.ps1 once, then scripts/build.ps1. scripts/package.ps1 builds/tests fresh staging and creates portable/source ZIPs and SHA-256 checksums from committed sources. A GitHub Windows workflow builds these assets on pushes, pull requests, version tags and manual runs.

The program is unsigned. Version resources identify the project/publisher nickname but do not create a trusted code-signing identity or remove SmartScreen warnings. No security setting is changed by the app.

The format coverage and requirement audit documents distinguish native identification, actual metadata extraction, deep validation and remaining limits. Final packaging, hosted workflow and interactive acceptance checks are pending; this document does not assert that the candidate has been published.

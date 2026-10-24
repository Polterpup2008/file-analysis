# File Analysis 1.1.0

This update focuses on camera and phone recognition. It adds an offline catalogue of 50,930 model/name relationships, including 170 Apple hardware identifiers; preserves recorded model codes alongside recognised names; and recovers source-linked vendor body IDs and DNG UniqueCameraModel fields. Exact manufacturer/model pairing, multiple-match and unknown states, source references and catalogue fingerprints remain visible.

The app uses no online device lookup and never infers a capture device from pixels, filenames, editors or lenses. A name match is a metadata claim, not proof of the physical hardware or photographer. The Google catalogue includes other Android equipment, and the snapshot cannot cover every model. Unknown vendor IDs and absent/failed readers remain separate states.

Extract the entire Windows x64 ZIP and run file-analysis.exe. Keep exiftool, c2patool and data folders beside it. No build tools or administrator rights are required; the executable remains unsigned. Source builds use scripts/bootstrap-dependencies.ps1 once, then scripts/build.ps1. Model-catalogue regeneration is separate and requires the recorded checksum-pinned inputs.

JSON schema 6 adds photo.recognition and compact recognized_device_names in share summaries. Full exports retain raw/numeric metadata; originals remain unchanged. See AUDIT-1.1.md for acceptance evidence and GUI-TESTING.md for actual desktop checks.

Published stable release: https://github.com/Polterpup2008/file-analysis/releases/tag/v1.1.0

The release was built from 1f75bf854dac9807975cad4644d0dd0a5a64787f after all eleven suites passed locally and on GitHub Windows. Final portable execution, source archive equality and a fresh public Windows download were verified. Windows ZIP SHA-256: 992fc775ea68b65b0bb4c75e83fcd97a24726c6e28de2459189f866aa4a852c6. The release includes source and SHA256-1.1.0.txt assets.

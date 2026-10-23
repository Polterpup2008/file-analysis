# File Analysis 0.4.0

Camera and capture information is now readable in Simple view. When present in the file, the app shows its reported camera/phone manufacturer and model, lens, capture time and original timezone offset, exposure/aperture/ISO/focal length, flash/shooting modes, camera/lens serial numbers and GPS coordinates.

Device identities from relevant EXIF, XMP, MakerNotes and QuickTime metadata retain their exact source fields. Different recorded identities produce a disagreement finding; missing model information and unavailable extraction have separate messages. The app does not guess a device from the image, a filename, editor name or lens, and it does not convert internal model codes into guessed product names.

Detailed view includes a Photo / device category with the summary and original source/numeric evidence. JSON schema 3 adds a structured photo section. JSON, CSV and text exports preserve capture evidence. Original files remain unchanged and analysis remains offline.

Camera tags are editable claims, not proof of the physical capture device or photographer. Missing tags do not establish AI generation. Local-path redaction does not remove GPS or device serial numbers. The executable remains unsigned.

Download the Windows x64 ZIP, extract the entire archive, then run file-analysis.exe. Keep its exiftool and c2patool folders beside it. The source archive is for developers; scripts/build.ps1 -Package builds and installs the portable application.

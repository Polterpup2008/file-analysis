# File Analysis 0.5.0

Simple view now includes an origin summary that brings together recorded capture-device tags, AI provenance claims, recovered generation settings and software names. Camera and generation metadata can coexist; the summary does not force a human/AI classification or infer a complete editing history. Cryptographic integrity and signer trust remain separate.

A privacy report highlights recovered location tags, device/document identifiers, names/contact/rights claims, preview tags and free text to review. Findings link back to their original fields. This is a selected-field review, not an exhaustive detector of personal information.

The new Share report button exports a compact JSON, CSV or text summary. Its native dialog independently controls omission of location/GPS, device/document identifiers, and names/contact/rights fields. All are omitted by default. Source filenames/paths, raw parser objects, binary blocks, complete provenance, prompts/workflows, free text and raw error messages are excluded from share summaries. Error and cancellation counts remain explicit. Full Export Results continues to preserve complete evidence.

Share summaries are not guaranteed anonymization. Retained dates, camera/lens/software values and file hashes can still identify a file or contain personal information. Review exported content before sharing. No original file is changed and no data is sent to a service.

Download the Windows x64 ZIP, extract the entire archive and run file-analysis.exe. Keep the exiftool and c2patool folders beside it. The executable remains unsigned.
